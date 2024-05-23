#include <stdint.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>

#include "iconv.h"

#ifdef HAVE_ICONV

// NOTE:
//      utf-8 may need more spaces to represent same char than wchar_t (utf-16),
//      recommended allocating double of char length for utf-8 spaces
extern inline int iconv_wc2utf8(wchar_t *in, size_t in_bytes, char *out, size_t out_bytes)
{
        return iconv_convert(in, in_bytes, ICONV_WCHAR, ICONV_UTF8, out, out_bytes);
}

extern inline int iconv_utf82wc(char *in, size_t in_bytes, wchar_t *out, size_t out_bytes)
{
        return iconv_convert(in, in_bytes, ICONV_UTF8, ICONV_WCHAR, out, out_bytes);
}

#else

extern inline int iconv_wc2utf8(wchar_t *in, size_t in_bytes, char *out, size_t out_bytes)
{
        UNUSED_PARAM(in);
        UNUSED_PARAM(in_bytes);
        UNUSED_PARAM(out);
        UNUSED_PARAM(out_bytes);

        return -EINVAL;
}

extern inline int iconv_utf82wc(char *in, size_t in_bytes, wchar_t *out, size_t out_bytes)
{
        UNUSED_PARAM(in);
        UNUSED_PARAM(in_bytes);
        UNUSED_PARAM(out);
        UNUSED_PARAM(out_bytes);

        return -EINVAL;
}

#endif