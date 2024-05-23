#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "compiler.h"
#include "string.h"
#include "logging.h"
#include "iconv.h"
#include "opts.h"

#define for_each_opt(i) for ((i) = &__start_section_opt_desc; (i) != &__stop_section_opt_desc; (i)++)

extern const opt_desc_t __start_section_opt_desc, __stop_section_opt_desc;

static int helptxt_default_value_print = 1;

void opts_helptxt_defval_print(int enabled)
{
        helptxt_default_value_print = !!enabled;
}

static size_t opt_data_sz[] = {
        [OPT_DATA_INT]    = sizeof(int64_t),
        [OPT_DATA_UINT]   = sizeof(int64_t),
        [OPT_DATA_FLOAT]  = sizeof(float),
        [OPT_DATA_DOUBLE] = sizeof(double),
};

static int is_valid_short_opt(const opt_desc_t *o)
{
        if (!o->short_opt || o->short_opt[0] == '\0')
                return 0;

        if (!isalpha(o->short_opt[0]) && !isdigit(o->short_opt[0]))
                return 0;

        return 1;
}

static char *opt_fmt_generate(void)
{
        char *fmt = NULL;
        const opt_desc_t *i;
        size_t cnt = 0, c = 0;

        for_each_opt(i) {
                if (!is_valid_short_opt(i))
                        continue;

                cnt++;

                if (i->has_arg == required_argument)
                        cnt++; // :
        }

        if (cnt == 0)
                return NULL;

        fmt = calloc(cnt + 2, sizeof(char));
        if (!fmt)
                return NULL;

        for_each_opt(i) {
                if (!is_valid_short_opt(i))
                        continue;

                if (c + ((i->has_arg == required_argument) ? 2 : 1) > cnt) {
                        pr_err("allocated fmt buffer cannot hold anymore, bugged\n");
                        goto err_free;
                }

                fmt[c++] = i->short_opt[0];

                if (i->has_arg == required_argument)
                        fmt[c++] = ':';
        }

        return fmt;

err_free:
        free(fmt);

        return NULL;
}

static struct option *longopts_generate(size_t *alloc_cnt)
{
        struct option *long_opts;
        const opt_desc_t *i;
        size_t cnt = 0, c = 0;

        for_each_opt(i) {
                if (i->long_opt && !i->long_opt[0])
                        continue;

                cnt++;
        }

        if (cnt == 0)
                return NULL;

        // struct option [] must end with extra empty slot
        long_opts = calloc(sizeof(struct option), cnt + 1);
        if (!long_opts)
                return NULL;

        for_each_opt(i) {
                struct option *o = &long_opts[c];

                if (i->long_opt && !i->long_opt[0])
                        continue;

                o->name = i->long_opt;
                o->has_arg = i->has_arg;
                o->flag = NULL;
                o->val = (is_valid_short_opt(i)) ? i->short_opt[0] : 0;

                c++;
        }

        if (alloc_cnt)
                *alloc_cnt = cnt;

        return long_opts;
}

static const opt_desc_t *opt_desc_find(char *sopt, const char *lopt)
{
        const opt_desc_t *i;

        for_each_opt(i) {
                if (sopt && is_valid_short_opt(i)) {
                        if (*sopt == i->short_opt[0])
                                return i;
                }

                if (lopt && (i->long_opt && i->long_opt[0])) {
                        if (is_str_equal((char *)lopt, i->long_opt, 0))
                                return i;
                }
        }

        return NULL;
}

static int opt_noarg_handle(const opt_desc_t *d)
{
        if (!d->data)
                return 0;

        if (0 == d->data_sz)
                return -EINVAL;

        memcpy(d->data, d->to_set, d->data_sz);

        return 0;
}

static int opt_strbuf_handle(const opt_desc_t *d, char *arg)
{
        if (0 == d->data_sz)
                return -EINVAL;

        strncpy(d->data, arg, d->data_sz);

        return 0;
}

static int opt_strptr_handle(const opt_desc_t *d, char *arg)
{
        *(char **)d->data = arg;

        return 0;
}

static int opt_num_handle(const opt_desc_t *d, char *arg)
{
        void *_buf;
        size_t buf_sz;
        int err = 0;

        if (0 == d->data_sz)
                return -EINVAL;

        buf_sz = opt_data_sz[d->data_type];
        _buf = calloc(buf_sz, sizeof(uint8_t));
        if (!_buf)
                return -ENOMEM;

        switch (d->data_type) {
        case OPT_DATA_INT: {
                int64_t *buf = _buf;

                *buf = strtoll(arg, NULL, d->int_base ? d->int_base : 10);
                if (errno == ERANGE) {
                        pr_err("failed to format \"%s\"\n", arg);
                        err = -errno;
                        goto out;
                }

                ptr_word_write(d->data, d->data_sz, *buf);;

                break;
        }

        case OPT_DATA_UINT: {
                uint64_t *buf = _buf;

                *buf = strtoull(arg, NULL, d->int_base ? d->int_base : 10);
                if (errno == ERANGE) {
                        pr_err("failed to format \"%s\"\n", arg);
                        err = -errno;
                        goto out;
                }

                ptr_unsigned_word_write(d->data, d->data_sz, *buf);

                break;
        }

        case OPT_DATA_FLOAT: {
                float *buf = _buf;

                *buf = strtof(arg, NULL);
                if (errno == ERANGE) {
                        pr_err("failed to format \"%s\"\n", arg);
                        err = -errno;
                        goto out;
                }

                if (d->data_sz < sizeof(float)) {
                        pr_err("data size cannot hold float type\n");
                        err = -EINVAL;
                        goto out;
                }

                memcpy(d->data, buf, sizeof(float));

                break;
        }

        case OPT_DATA_DOUBLE: {
                double *buf = _buf;

                *buf = strtod(arg, NULL);
                if (errno == ERANGE) {
                        pr_err("failed to format \"%s\"\n", arg);
                        err = -errno;
                        goto out;
                }

                if (d->data_sz < sizeof(double)) {
                        pr_err("data size cannot hold float type\n");
                        err = -EINVAL;
                        goto out;
                }

                memcpy(d->data, buf, sizeof(double));

                break;
        }

        default:
                err = -EINVAL;
                goto out;
        }

out:
        if (_buf)
                free(_buf);

        return err;
}

static int opt_optstr_handle(const opt_desc_t *d, char *arg)
{
        size_t i = 0;

        for (const opt_val_t *s = d->optvals; s->str_val != NULL; s++, i++) {
                if (is_str_equal((char *)s->str_val, arg, 0)) {
                        ptr_unsigned_word_write(d->data, d->data_sz, (uint64_t){ i });
                        return 0;
                }
        }

        pr_rawlvl(ERROR, "unknown value for \"%s\"\n", arg);

        return -EINVAL;
}

static int opt_needarg_handle(const opt_desc_t *d, char *arg)
{
        if (!d->data)
                return -EINVAL;

        switch (d->data_type) {
        case OPT_DATA_UINT:
                if (d->optvals)
                        return opt_optstr_handle(d, arg);

                __attribute__((fallthrough));
        case OPT_DATA_INT:
        case OPT_DATA_FLOAT:
        case OPT_DATA_DOUBLE:
                return opt_num_handle(d, arg);

        case OPT_DATA_STRBUF:
                return opt_strbuf_handle(d, arg);

        case OPT_DATA_STRPTR:
                return opt_strptr_handle(d, arg);

        case OPT_DATA_GENERIC:
        default:
                return -EINVAL;
        }
}

static int opt_desc_handle(const opt_desc_t *d, char *arg)
{
        switch (d->has_arg) {
        case no_argument:
                return opt_noarg_handle(d);

        case required_argument:
                return opt_needarg_handle(d, arg);

        default:
                return -EINVAL;
        }
}

static __always_inline void optval_help_print(const opt_desc_t *d, size_t param_len)
{
        size_t i = 0;
        uint64_t x;

        if (d->data) {
                ptr_unsigned_word_read(d->data, d->data_sz, &x);
        }

        for (const opt_val_t *v = d->optvals; v && v->str_val; v++, i++) {
                pr_color(FG_LT_GREEN, "    %-*s    %s",
                         (int)param_len, "", v->str_val);

                if (v->desc)
                        pr_color(FG_GREEN, ": %s", v->desc);

                if (d->data && i == x) {
                        pr_color(FG_LT_BLUE, " (default)");
                }

                pr_color(FG_GREEN, "\n");
        }
}

static __always_inline void opt_default_val_print(const opt_desc_t *d)
{
        uint32_t printable =
                BIT(OPT_DATA_INT) |
                BIT(OPT_DATA_UINT) |
                BIT(OPT_DATA_FLOAT) |
                BIT(OPT_DATA_DOUBLE) |
                BIT(OPT_DATA_STRBUF);

        if (!d->data)
                return;

        if (0 == (BIT(d->data_type) & printable))
                return;

        if (d->data_type == OPT_DATA_STRBUF && is_strptr_not_set((char *)d->data))
                return;

        pr_color(FG_LT_BLUE, " ( ");

        switch (d->data_type) {
        case OPT_DATA_INT:
                pr_color(FG_LT_BLUE, "%jd", (int64_t){ ({ int64_t x; ptr_signed_word_read(d->data, d->data_sz, &x); x; })});
                break;
        case OPT_DATA_UINT:
                pr_color(FG_LT_BLUE, "%ju", (uint64_t){ ({ uint64_t x; ptr_unsigned_word_read(d->data, d->data_sz, &x); x; })});
                break;
        case OPT_DATA_FLOAT:
                pr_color(FG_LT_BLUE, "%.4f", *(float *)d->data);
                break;
        case OPT_DATA_DOUBLE:
                pr_color(FG_LT_BLUE, "%.4lf", *(double *)d->data);
                break;
        case OPT_DATA_STRBUF:
                pr_color(FG_LT_BLUE, "\"%s\"", (char *)d->data);
                break;
        }

        pr_raw(" )");
}

static void longopts_help(void)
{
        const opt_desc_t *i;
        char *buf;
        size_t buf_len;
        size_t s = 0;

        // find max param text length
        for_each_opt(i) {
                size_t t = 0;

                if (is_valid_short_opt(i)) {
                        t += 1 + 1 + 1; // '-' 'o' ' '
                }

                if (i->long_opt && i->long_opt[0]) {
                        t += 2 + strlen(i->long_opt) + 1; // '--' 'longopt' ' '
                }

                if (i->has_arg == required_argument) {
                        t += 4; // '<..>'
                }

                // pick longest
                if (t > s)
                        s = t;
        }

        buf_len = s + 8; // 8 whitespaces
        buf = malloc(buf_len);
        if (!buf) {
                pr_err("failed to allocate buffer\n");
                return;
        }

        for_each_opt(i) {
                size_t len = 0;

                memset(buf, '\0', buf_len);

                if (is_valid_short_opt(i))
                        len += snprintf(buf + len, buf_len - len, "-%c ", i->short_opt[0]);

                if (i->long_opt && i->long_opt[0])
                        len += snprintf(buf + len, buf_len - len, "--%s ", i->long_opt);

                if (i->has_arg == required_argument)
                        len += snprintf(buf + len, buf_len - len, "<..>");

                pr_color(FG_CYAN, "    %-*s", (int)buf_len, buf);

                if (!i->help) {
                        goto print_optval;
                }

                {
                        char *c = strchr(i->help, '\n');
                        if (!c) {
                                pr_color(FG_GREEN, "%s", i->help);
                                goto print_optval;
                        }
                }

                {
                        size_t l = strlen(i->help);
                        char *h = calloc(l + 2, sizeof(char));
                        char *p = h;
                        if (!h) {
                                pr_err("failed to allocate help buffer\n");
                                return;
                        }

                        strncpy(h, i->help, l);

                        while (1) {
                                char *c = strchr(p, '\n');
                                if (c)
                                        c[0] = '\0';

                                pr_color(FG_GREEN, "%s\n", p);

                                if (c) {
                                        pr_color(FG_GREEN, "    %-*s", (int)buf_len, "");
                                        p = c + 1;

                                        continue;
                                }

                                break;
                        }

                        free(h);
                }

print_optval:
                if (i->optvals) {
                        pr_raw("\n");
                        optval_help_print(i, buf_len);

                        continue;
                }

                if (helptxt_default_value_print)
                        opt_default_val_print(i);

                pr_raw("\n");
        }
}

#if defined __WINNT__ && defined SUBSYS_WINDOW
static void longopts_help_messagebox(void)
{
        const opt_desc_t *i;
        char *line, *buf = NULL;
        size_t line_len, buf_len, buf_pos;
        size_t s = 0;

        // find max param text length
        for_each_opt(i) {
                size_t t = 0;

                if (is_valid_short_opt(i)) {
                        t += 1 + 1 + 1; // '-' 'o' ' '
                }

                if (i->long_opt && i->long_opt[0]) {
                        t += 2 + strlen(i->long_opt) + 1; // '--' 'longopt' ' '
                }

                if (i->has_arg == required_argument) {
                        t += 4; // '<..>'
                }

                // pick longest
                if (t > s)
                        s = t;
        }

        line_len = s + 8; // 8 whitespaces
        line = malloc(line_len);
        if (!line) {
                pr_err("failed to allocate line buffer\n");
                return;
        }

        buf_pos = 0;
        buf_len = 4096;
        buf = calloc(buf_len, sizeof(char));
        if (!buf) {
                pr_err("failed to allocate buffer\n");
                return;
        }

        for_each_opt(i) {
                size_t len = 0;

                memset(line, '\0', line_len);

                if (is_valid_short_opt(i))
                        len += snprintf(line + len, line_len - len, "-%c ", i->short_opt[0]);

                if (i->long_opt && i->long_opt[0])
                        len += snprintf(line + len, line_len - len, "--%s ", i->long_opt);

                if (i->has_arg == required_argument)
                        len += snprintf(line + len, line_len - len, "<..>");

                snprintf_resize(&buf, &buf_pos, &buf_len, "    %-*s", (int)line_len, line);

                if (!i->help) {
                        goto print_optval;
                }

                {
                        char *c = strchr(i->help, '\n');
                        if (!c) {
                                snprintf_resize(&buf, &buf_pos, &buf_len, "%s ", i->help);
                                goto print_optval;
                        }
                }

                {
                        size_t l = strlen(i->help);
                        char *h = calloc(l + 2, sizeof(char));
                        char *p = h;
                        if (!h) {
                                pr_err("failed to allocate help buffer\n");
                                return;
                        }

                        strncpy(h, i->help, l);

                        while (1) {
                                char *c = strchr(p, '\n');
                                if (c)
                                        c[0] = '\0';

                                snprintf_resize(&buf, &buf_pos, &buf_len, "%s\n", p);

                                if (c) {
                                        snprintf_resize(&buf, &buf_pos, &buf_len,  "    %-*s", (int)line_len, "");
                                        p = c + 1;

                                        continue;
                                }

                                break;
                        }

                        free(h);
                }

print_optval:
                if (i->optvals) {
                        size_t _i = 0;
                        uint64_t x;

                        if (i->data) {
                                ptr_unsigned_word_read(i->data, i->data_sz, &x);
                        }

                        snprintf_resize(&buf, &buf_pos, &buf_len, "\n");

                        for (const opt_val_t *v = i->optvals; v && v->str_val; v++, _i++) {
                                snprintf_resize(&buf, &buf_pos, &buf_len, "    %-*s    %s", (int)line_len, "", v->str_val);

                                if (v->desc)
                                        snprintf_resize(&buf, &buf_pos, &buf_len, ": %s", v->desc);

                                if (i->data && _i == x) {
                                        snprintf_resize(&buf, &buf_pos, &buf_len, " (default)");
                                }

                                snprintf_resize(&buf, &buf_pos, &buf_len, "\n");
                        }
                } else {
                        if (helptxt_default_value_print) {
                                do {
                                        uint32_t printable =
                                                BIT(OPT_DATA_INT) |
                                                BIT(OPT_DATA_UINT) |
                                                BIT(OPT_DATA_FLOAT) |
                                                BIT(OPT_DATA_DOUBLE) |
                                                BIT(OPT_DATA_STRBUF);

                                        if (!i->data)
                                                break;

                                        if (0 == (BIT(i->data_type) & printable))
                                                break;

                                        snprintf_resize(&buf, &buf_pos, &buf_len, " ( ");

                                        switch (i->data_type) {
                                        case OPT_DATA_INT:
                                                snprintf_resize(&buf, &buf_pos, &buf_len, "%jd", (int64_t){ ({ int64_t x; ptr_signed_word_read(i->data, i->data_sz, &x); x; })});
                                                break;
                                        case OPT_DATA_UINT:
                                                snprintf_resize(&buf, &buf_pos, &buf_len, "%ju", (uint64_t){ ({ uint64_t x; ptr_unsigned_word_read(i->data, i->data_sz, &x); x; })});
                                                break;
                                        case OPT_DATA_FLOAT:
                                                snprintf_resize(&buf, &buf_pos, &buf_len, "%.4f", *(float *)i->data);
                                                break;
                                        case OPT_DATA_DOUBLE:
                                                snprintf_resize(&buf, &buf_pos, &buf_len, "%.4lf", *(double *)i->data);
                                                break;
                                        case OPT_DATA_STRBUF:
                                                snprintf_resize(&buf, &buf_pos, &buf_len, "\"%s\"", (char *)i->data);
                                                break;
                                        }

                                        snprintf_resize(&buf, &buf_pos, &buf_len, " )");
                                } while (0);
                        }

                        snprintf_resize(&buf, &buf_pos, &buf_len, "\n");
                }
        }

        mb_printf("HELP", MB_OK, "%s", buf);

        free(line);
        free(buf);
}
#endif

void print_help(void)
{
#if defined __WINNT__ && defined SUBSYS_WINDOW
        if (is_console_allocated() && !is_console_hid()) {
                                longopts_help();
                        } else {
                                longopts_help_messagebox();
                        }

#else
        longopts_help();
#endif
}

int longopts_parse(int argc, char *argv[], void nonopt_cb(char *arg))
{
        struct option *lopts;
        size_t lopts_cnt = 0;
        char *optfmt;
        int err = 0;

        // TODO: validate options

        // keeps getopt silent on error
        // opterr = 0;

        optfmt = opt_fmt_generate();
        lopts = longopts_generate(&lopts_cnt);

        if (!optfmt && !lopts) {
                pr_dbg("no cmdline options defined\n");
                return 0;
        }

        if (!optfmt)
                optfmt = "";

        while (1) {
                const opt_desc_t *desc;
                int optidx = -1, c = -1;
                c = getopt_long(argc, argv, optfmt, lopts, &optidx);

                if (c == -1 && optidx == -1)
                        break;

                if (c == '?') {
                        err = -EINVAL;

                        goto out;
                }

                if (c == 'h' || (optidx >= 0 && is_str_equal((char *)lopts[optidx].name, "help", 0))) {
                        print_help();
                        err = -EAGAIN;
                        goto out;
                }

                desc = opt_desc_find((isalpha(c) || isdigit(c)) ? &(char){ c } : NULL,
                                     (optidx >= 0 && optidx < (int)lopts_cnt) ? lopts[optidx].name : NULL);
                if (!desc) {
                        pr_rawlvl(ERROR, "unknown ");

                        if (isalpha(c) || isdigit(c))
                                pr_rawlvl(ERROR, "short option \'-%c\' ", c);

                        if (optidx >= 0)
                                pr_rawlvl(ERROR, "long option \'--%s\' ", lopts[optidx].name);

                        pr_rawlvl(ERROR, "\n");

                        err = -EINVAL;
                        goto out;
                }

                if ((err = opt_desc_handle(desc, optarg)))
                        goto out;
        }

        if (optind < argc && nonopt_cb) {
                while (optind < argc)
                        nonopt_cb(argv[optind++]);
        }

out:
        if (optfmt)
                free(optfmt);

        if (lopts)
                free(lopts);

        return err;
}

#ifdef HAVE_ICONV
int wchar_longopts_parse(int argc, wchar_t *wargv[], void nonopt_cb(char *arg))
{
        char **argv;
        int ret = 0;

        // extra null terminated is required for getopt_long()
        argv = calloc(argc + 1, sizeof(char *));
        if (!argv)
                return -ENOMEM;

        for (int i = 0; i < argc; i++) {
                char **v = &argv[i];
                size_t len = wcslen(wargv[i]);
                *v = calloc(len + 2, sizeof(char));
                if (!*v)
                        return -ENOMEM;

                if (iconv_wc2utf8(wargv[i], len * sizeof(wchar_t), *v, len * sizeof(char)))
                        return -EINVAL;
        }

        ret = longopts_parse(argc, argv, nonopt_cb);

        for (int i = 0; i < argc; i++) {
                if (argv[i])
                        free(argv[i]);
        }

        free(argv);

        return ret;
}
#else
int wchar_longopts_parse(int argc, wchar_t *wargv[], void nonopt_cb(char *arg))
{
        UNUSED_PARAM(argc);
        UNUSED_PARAM(wargv);
        UNUSED_PARAM(nonopt_cb);

        return -EINVAL;
}
#endif

lsopt_noarg(h, help, NULL, 0, NULL, "This help message");
