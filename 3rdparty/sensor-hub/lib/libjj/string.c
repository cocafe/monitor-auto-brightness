#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#include "compiler.h"
#include "bits.h"
#include "malloc.h"
#include "string.h"

int is_str_equal(char *a, char *b, int caseless)
{
        enum {
                NOT_EQUAL = 0,
                EQUAL = 1,
        };

        size_t len_a;
        size_t len_b;

        if (!a || !b)
                return NOT_EQUAL;

        if (unlikely(a == b))
                return EQUAL;

        len_a = strlen(a);
        len_b = strlen(b);

        if (len_a != len_b)
                return NOT_EQUAL;

        if (caseless) {
                if (!strncasecmp(a, b, len_a))
                        return EQUAL;

                return NOT_EQUAL;
        }

        if (!strncmp(a, b, len_a))
                return EQUAL;

        return NOT_EQUAL;
}

int is_wstr_equal(wchar_t *a, wchar_t *b)
{
        enum {
                NOT_EQUAL = 0,
                EQUAL = 1,
        };

        size_t len_a;
        size_t len_b;

        if (!a || !b)
                return NOT_EQUAL;

        if (unlikely(a == b))
                return EQUAL;

        len_a = wcslen(a);
        len_b = wcslen(b);

        if (len_a != len_b)
                return NOT_EQUAL;

        if (!wcsncmp(a, b, len_a))
                return EQUAL;

        return NOT_EQUAL;
}

int vprintf_resize(char **buf, size_t *pos, size_t *len, const char *fmt, va_list arg)
{
        va_list arg2;
        char cbuf;
        char *sbuf = *buf;
        int _len, ret;

        va_copy(arg2, arg);
        _len = vsnprintf(&cbuf, sizeof(cbuf), fmt, arg2);
        va_end(arg2);

        if (_len < 0)
                return _len;

        if (!sbuf) {
                size_t append_len = _len + 2;

                sbuf = calloc(append_len, sizeof(char));
                if (!sbuf)
                        return -ENOMEM;

                *buf = sbuf;
                *len = append_len;
                *pos = 0;
        } else {
                size_t append_len = _len + 2;
                size_t new_len = *len + append_len;

                // do realloc
                if (*pos + append_len > *len) {
                        sbuf = realloc_safe(*buf, *len, new_len);
                        if (!sbuf)
                                return -ENOMEM;

                        *buf = sbuf;
                        *len = new_len;
                }
        }

        sbuf = &((*buf)[*pos]);

        ret = vsnprintf(sbuf, *len - *pos, fmt, arg);
        if (ret < 0) {
                return ret;
        }

        *pos += ret;

        return ret;
}

int snprintf_resize(char **buf, size_t *pos, size_t *len, const char *fmt, ...)
{
        va_list ap;
        int ret;

        va_start(ap, fmt);
        ret = vprintf_resize(buf, pos, len, fmt, ap);
        va_end(ap);

        return ret;
}

static const char *quad_bits_rep[] = {
        [0x00] = "0000",
        [0x01] = "0001",
        [0x02] = "0010",
        [0x03] = "0011",
        [0x04] = "0100",
        [0x05] = "0101",
        [0x06] = "0110",
        [0x07] = "0111",
        [0x08] = "1000",
        [0x09] = "1001",
        [0x0A] = "1010",
        [0x0B] = "1011",
        [0x0C] = "1100",
        [0x0D] = "1101",
        [0x0E] = "1110",
        [0x0F] = "1111",
};

int bin_snprintf(char *str, size_t slen, uint64_t val, size_t vsize)
{
        size_t c = 0;

        val = htobe64(val);

        if (!str || !slen || !vsize)
                return -EINVAL;

        // snprintf() will always put an extra '\0' at the end of buffer
        if (vsize > sizeof(uint64_t) || slen < (vsize * BITS_PER_BYTE + 1))
                return -ERANGE;

        for (size_t i = sizeof(uint64_t) - vsize; i < sizeof(uint64_t); i++) {
                uint8_t byte_hi, byte_lo;

                byte_hi = ((uint8_t *)&val)[i] & 0xf0U >> 4U;
                byte_lo = ((uint8_t *)&val)[i] & 0x0fU;

                snprintf(&str[c], slen - c, "%s%s",
                         quad_bits_rep[byte_hi], quad_bits_rep[byte_lo]);
                c += 8;
        }

        return 0;
}

int __str_cat(char *dest, size_t dest_sz, char *src)
{
        // strncat() require @dest has space
        // more than strlen(dest) + @n + 1
        size_t s = strlen(dest) + strlen(src) + 1;

        if (s > dest_sz) {
                return -E2BIG;
        }

        // strncat() return address to @dest
        strncat(dest, src, strlen(src));

        return 0;
}

char *__str_ncpy(char *dest, const char *src, size_t len)
{
        char *ret;

        ret = strncpy(dest, src, len - 1);
        dest[len - 1] = '\0';

        return ret;
}

wchar_t *__wstr_ncpy(wchar_t *dest, const wchar_t *src, size_t len)
{
        wchar_t *ret;

        ret = wcsncpy(dest, src, len);
        dest[len - 1] = L'\0';

        return ret;
}
