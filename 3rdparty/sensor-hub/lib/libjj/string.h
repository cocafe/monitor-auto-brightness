#ifndef __JJ_STRING_H__
#define __JJ_STRING_H__

#include <stdint.h>
#include <errno.h>
#include <wchar.h>
#include <string.h>

/* Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */
#define __stringify_1(x...)	#x
#define __stringify(x...)	__stringify_1(x)

#define is_strptr_not_set(s)    ((s) == NULL || (s)[0] == '\0')
#define is_strptr_set(s)        (!is_strptr_not_set((s)))

#define WCBUF_LEN               ARRAY_SIZE
#define WCSLEN_BYTE(s)          (wcslen(s) * sizeof(wchar_t))

int bin_snprintf(char *str, size_t slen, uint64_t val, size_t vsize);
int vprintf_resize(char **buf, size_t *pos, size_t *len, const char *fmt, va_list arg);
int snprintf_resize(char **buf, size_t *pos, size_t *len, const char *fmt, ...);

int __str_cat(char *dest, size_t dest_sz, char *src);
char *__str_ncpy(char *dest, const char *src, size_t len);
// @len: wide characters count
wchar_t *__wstr_ncpy(wchar_t *dest, const wchar_t *src, size_t len);

int is_str_equal(char *a, char *b, int caseless);
int is_wstr_equal(wchar_t *a, wchar_t *b);

#endif // __JJ_STRING_H__