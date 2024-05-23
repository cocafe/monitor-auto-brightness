#ifndef __JJ_OPTS_H__
#define __JJ_OPTS_H__

#include <stdint.h>

#include "compiler.h"

#if defined(__MINGW64__) || defined(__MINGW32__) || defined(__WINNT__)
#include "getopt_musl.h"
#else
#include <getopt.h>
#endif

typedef struct opt_val opt_val_t;
typedef struct opt_desc opt_desc_t;

enum opt_data_type {
        OPT_DATA_INT = 0,
        OPT_DATA_UINT,
        OPT_DATA_FLOAT,
        OPT_DATA_DOUBLE,
        OPT_DATA_STRBUF,
        OPT_DATA_STRPTR,
        OPT_DATA_GENERIC,
        NUM_OPT_DATA_TYPES,
};

// MUST end with { NULL, NULL }
struct opt_val {
        const char *str_val;
        const char *desc;
};

struct opt_desc {
        char                   *short_opt;
        char                   *long_opt;
        int                     has_arg;
        void                   *data;
        size_t                  data_sz;
        uint32_t                data_type;
        uint32_t                int_base;
        void                   *to_set;
        const opt_val_t        *optvals;
        const char             *help;       // use '\n' to make multiple lines
};

#define __opt_entry(_sopt, _lopt, _has_arg, _ref, _ref_sz, _ref_type, _int_base, _set, _optval, _help) \
{                                                                                                        \
        .short_opt      = #_sopt,                                                                        \
        .long_opt       = #_lopt,                                                                        \
        .has_arg        = _has_arg,                                                                      \
        .data           = _ref,                                                                          \
        .data_sz        = _ref_sz,                                                                       \
        .data_type      = _ref_type,                                                                     \
        .int_base       = _int_base,                                                                     \
        .to_set         = _set,                                                                          \
        .optvals        = _optval,                                                                       \
        .help           = _help,                                                                         \
}

#define ___sopt_entry(sopt, need_arg, ref, ref_sz, ref_type, int_base, set, optval, help)       \
const opt_desc_t opt_##sopt __ALIGNED(sizeof(void *)) __SECTION("section_opt_desc") =           \
        __opt_entry(sopt, \0, need_arg, ref, ref_sz, ref_type, int_base, set, optval, help)

#define __sopt_hex_entry(sopt, need_arg, ref, ref_sz, ref_type, set, optval, help) \
                ___sopt_entry(sopt, need_arg, ref, ref_sz, ref_type, 16, set, optval, help)

#define __sopt_entry(sopt, need_arg, ref, ref_sz, ref_type, set, optval, help) \
                ___sopt_entry(sopt, need_arg, ref, ref_sz, ref_type, 0, set, optval, help)

#define sopt_int(sopt, ref, ref_sz, help) \
                __sopt_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_INT, NULL, NULL, help)

#define sopt_uint(sopt, ref, ref_sz, help) \
                __sopt_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_UINT, NULL, NULL, help)

#define sopt_uint_hex(sopt, ref, ref_sz, help) \
                __sopt_hex_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_UINT, NULL, NULL, help)

#define sopt_flt(sopt, ref, ref_sz, help) \
                __sopt_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_FLOAT, NULL, NULL, help)

#define sopt_dbl(sopt, ref, ref_sz, help) \
                __sopt_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_DOUBLE, NULL, NULL, help)

#define sopt_strbuf(sopt, ref, ref_sz, help) \
                __sopt_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_STRBUF, NULL, NULL, help)

#define sopt_strptr(sopt, ref, help) \
                __sopt_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_STRPTR, NULL, NULL, help)

#define sopt_optval(sopt, ref, ref_sz, optval, help) \
                __sopt_entry(sopt, required_argument, ref, ref_sz, OPT_DATA_UINT, NULL, optval, help)


#define sopt_noarg(sopt, ref, ref_sz, set, help) \
                __sopt_entry(sopt, no_argument, ref, ref_sz, OPT_DATA_GENERIC, set, NULL, help)


#define ___lsopt_entry(sopt, lopt, need_arg, ref, ref_sz, ref_type, int_base, set, optval, help)        \
const opt_desc_t opt_##lopt __ALIGNED(sizeof(void *)) __SECTION("section_opt_desc") =                   \
        __opt_entry(sopt, lopt, need_arg, ref, ref_sz, ref_type, int_base, set, optval, help)

#define __lsopt_hex_entry(sopt, lopt, need_arg, ref, ref_sz, ref_type, set, optval, help) \
                ___lsopt_entry(sopt, lopt, need_arg, ref, ref_sz, ref_type, 16, set, optval, help)

#define __lsopt_entry(sopt, lopt, need_arg, ref, ref_sz, ref_type, set, optval, help) \
                ___lsopt_entry(sopt, lopt, need_arg, ref, ref_sz, ref_type, 0, set, optval, help)

#define lsopt_int(sopt, lopt, ref, ref_sz, help) \
                __lsopt_entry(sopt, lopt, required_argument, ref, ref_sz, OPT_DATA_INT, NULL, NULL, help)

#define lsopt_uint(sopt, lopt, ref, ref_sz, help) \
                __lsopt_entry(sopt, lopt, required_argument, ref, ref_sz, OPT_DATA_UINT, NULL, NULL, help)

#define lsopt_uint_hex(sopt, lopt, ref, ref_sz, help) \
                __lsopt_hex_entry(sopt, lopt, required_argument, ref, ref_sz, OPT_DATA_UINT, NULL, NULL, help)

#define lsopt_flt(sopt, lopt, ref, ref_sz, help) \
                __lsopt_entry(sopt, lopt, required_argument, ref, ref_sz, OPT_DATA_FLOAT, NULL, NULL, help)

#define lsopt_dbl(sopt, lopt, ref, ref_sz, help) \
                __lsopt_entry(sopt, lopt, required_argument, ref, ref_sz, OPT_DATA_DOUBLE, NULL, NULL, help)

#define lsopt_strbuf(sopt, lopt, ref, ref_sz, help) \
                __lsopt_entry(sopt, lopt, required_argument, ref, ref_sz, OPT_DATA_STRBUF, NULL, NULL, help)

#define lsopt_strptr(sopt, lopt, ref, help) \
                __lsopt_entry(sopt, lopt, required_argument, ref, 0, OPT_DATA_STRPTR, NULL, NULL, help)

#define lsopt_optval(sopt, lopt, ref, ref_sz, optval, help) \
                __lsopt_entry(sopt, lopt, required_argument, ref, ref_sz, OPT_DATA_UINT, NULL, optval, help)


#define lsopt_noarg(sopt, lopt, ref, ref_sz, set, help) \
                __lsopt_entry(sopt, lopt, no_argument, ref, ref_sz, OPT_DATA_GENERIC, set, NULL, help)


#define lopt_int(lopt, ref, ref_sz, help) \
                lsopt_int(\0, lopt, ref, ref_sz, help)

#define lopt_uint(lopt, ref, ref_sz, help) \
                lsopt_uint(\0, lopt, ref, ref_sz, help)

#define lopt_uint_hex(lopt, ref, ref_sz, help) \
                lsopt_uint_hex(\0, lopt, ref, ref_sz, help)

#define lopt_flt(lopt, ref, ref_sz, help) \
                lsopt_flt(\0, lopt, ref, ref_sz, help)

#define lopt_dbl(lopt, ref, ref_sz, help) \
                lsopt_dbl(\0, lopt, ref, ref_sz, help)

#define lopt_strbuf(lopt, ref, ref_sz, help) \
                lsopt_strbuf(\0, lopt, ref, ref_sz, help)

#define lopt_strptr(lopt, ref, help) \
                lsopt_strptr(\0, lopt, ref, help)

#define lopt_optval(lopt, ref, ref_sz, optval, help) \
                lsopt_optval(\0, lopt, ref, ref_sz, optval, help)


#define lopt_noarg(lopt, ref, ref_sz, set, help) \
                lsopt_noarg(\0, lopt, ref, ref_sz, set, help)

#define lopt_noarg_simple(var, set, help) \
                lsopt_noarg(\0, var, &(var), sizeof(var), &(typeof(var)){(set)}, help)

#define lopt_int_simple(var, help) \
                lsopt_int(\0, var, &(var), sizeof(var), help)

#define lopt_uint_simple(var, help) \
                lsopt_uint(\0, var, &(var), sizeof(var), help)

#define lopt_strbuf_simple(var, help) \
                lsopt_strbuf(\0, var, var, sizeof(var), help)

void opts_helptxt_defval_print(int enabled);
int longopts_parse(int argc, char *argv[], void nonopt_cb(char *arg));
int wchar_longopts_parse(int argc, wchar_t *wargv[], void nonopt_cb(char *arg));
void print_help(void);

#ifdef UNICODE
#define lopts_parse wchar_longopts_parse
#else
#define lopts_parse longopts_parse
#endif

#endif // __JJ_OPTS_H__