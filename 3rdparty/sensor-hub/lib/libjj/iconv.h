#ifndef __JJ_ICONV_H__
#define __JJ_ICONV_H__

#include <wchar.h>

#include "utils.h"

#define ICONV_UTF8              "UTF-8"
#define ICONV_CP936             "CP936"
#define ICONV_WCHAR             "UTF-16LE"

#ifdef __WINNT__
#define ICONV_WIN_WCHAR         "UTF-16LE"

#ifdef ICONV_WCHAR
#undef ICONV_WCHAR
#define ICONV_WCHAR             ICONV_WIN_WCHAR
#endif
extern int iconv_locale_ok;

char *iconv_locale_cp(void);
int iconv_winnt_locale_init(void);
int iconv_locale_to_utf8(char *in, size_t in_bytes, char *out, size_t out_bytes);
#endif // __WINNT__

int iconv_convert(void *in, size_t in_bytes, const char *in_encode, const char *out_encode, void *out, size_t out_bytes);
int iconv_strncmp(char *s1, char *c1, size_t l1, char *s2, char *c2, size_t l2, int *err);

int iconv_wc2utf8(wchar_t *in, size_t in_bytes, char *out, size_t out_bytes);
int iconv_utf82wc(char *in, size_t in_bytes, wchar_t *out, size_t out_bytes);

#endif // __JJ_ICONV_H__