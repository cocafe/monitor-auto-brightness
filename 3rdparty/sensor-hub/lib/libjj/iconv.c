#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iconv.h>

#include "iconv.h"

int iconv_locale_ok = 0;

#ifdef __WINNT__
#include <winnls.h>

char locale_cp[64] = { 0 };

int iconv_winnt_locale_init(void)
{
        iconv_t t;
        int err = 0;

        snprintf(locale_cp, sizeof(locale_cp), "CP%u", GetACP());

        t = iconv_open(ICONV_UTF8, locale_cp);
        if (t == (iconv_t)-1) {
                err = -ENOTSUP;
                iconv_locale_ok = 0;
        } else {
                iconv_locale_ok = 1;
                iconv_close(t);
        }

        return err;
}

int iconv_locale_to_utf8(char *in, size_t in_bytes, char *out, size_t out_bytes)
{
        if (iconv_locale_ok)
                return iconv_convert(in, in_bytes, locale_cp, ICONV_UTF8, out, out_bytes);

        return -EINVAL;
}

char *iconv_locale_cp(void)
{
        if (iconv_locale_ok)
                return locale_cp;

        return NULL;
}

#endif // __WINNT__

/**
 * @param in: should be (char *) or (wchar_t *)
 * @param in_bytes: input bytes, not the string char count (length)
 * @param in_encode: iconv code page name, e.g "utf8", "gb2312"
 * @param out_encode: iconv code page name
 * @param out: can be (char *) or (wchar_t *)
 * @param out_bytes: bytes that [out] can hold, recommended to allocate more
 *                   bytes than [in_bytes] for [out].
 *                   note that, some encodings (e.g. utf-8) may require more
 *                   spaces than utf-16 to represent some chars (e.g CJK chars),
 *                   allocate double space of wchar length for utf-8 if not sure.
 * @return 0 on success
 */
int iconv_convert(void *in, size_t in_bytes, const char *in_encode, const char *out_encode, void *out, size_t out_bytes)
{
        iconv_t cd;

        if (!in || !in_encode || !out_encode || !out || !in_bytes || !out_bytes)
                return -EINVAL;

        cd = iconv_open(out_encode, in_encode);
        if (cd == (iconv_t)-1) {
                if (errno == EINVAL)
                        return -ENOTSUP;

                return -errno;
        }

        iconv(cd, (char **)&in, &in_bytes, (char **)&out, &out_bytes);

        if (iconv_close(cd) != 0)
                return -EFAULT;

        return 0;
}

int iconv_strncmp(char *s1, char *c1, size_t l1, char *s2, char *c2, size_t l2, int *err)
{
        char *b1 = NULL;
        char *b2 = NULL;
        int ret = -EINVAL;
        int __err = 0;
        const int extra = 32;

        if (!s1 || !c1 || !s2 || !c2)
                return -EINVAL;

        if (strcasecmp(c1, ICONV_UTF8)) {
                b1 = calloc(l1 + extra, sizeof(char));
                if (!b1) {
                        __err = -ENOMEM;
                        goto out;
                }

                __err = iconv_convert(s1, l1, c1, ICONV_UTF8, b1, l1 + extra);
                if (__err)
                        goto out;

                s1 = b1;
                l1 += extra;
        }

        if (strcasecmp(c2, ICONV_UTF8)) {
                b2 = calloc(l2 + extra, sizeof(char));
                if (!b2) {
                        __err = -ENOMEM;
                        goto out;
                }

                __err = iconv_convert(s2, l2, c2, ICONV_UTF8, b2, l2 + extra);
                if (__err)
                        goto out;

                s2 = b2;
                l2 += extra;
        }

        ret = strncmp(s1, s2, (l1 > l2) ? l1 : l2);

out:
        if (b1)
                free(b1);

        if (b2)
                free(b2);

        if (err)
                *err = __err;

        return ret;
}
