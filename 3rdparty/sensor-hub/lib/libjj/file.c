#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "logging.h"

char *file_read(const char *path)
{
        char *buf = NULL;
        FILE *fp = 0;
        int64_t fsize = 0; // ssize_t may not suitable on 32bits

        fp = fopen(path, "rb");
        if (!fp) {
                pr_err("fopen(): %s: %s\n", path, strerror(errno));
                goto err;
        }

        if (fseek(fp, 0UL, SEEK_END)) {
                pr_err("fseek(): %s: %s\n", path, strerror(errno));
                goto err;
        }

        fsize = ftell(fp);
        if (fsize == -1) {
                pr_err("ftell(): %s: %s\n", path, strerror(errno));
                goto err;
        }

        if (fseek(fp, 0UL, SEEK_SET)) {
                pr_err("rewind(): %s: %s\n", path, strerror(errno));
                goto err;
        }

        buf = calloc(1, fsize + 2);
        if (!buf) {
                pr_err("failed to allocate memory size: %zu\n", (size_t)fsize);
                goto err;
        }

        if (fread(buf, fsize, 1, fp) != 1) {
                pr_err("fread(): %s: %s\n", path, strerror(ferror(fp)));
                goto err;
        }

        fclose(fp);

        return buf;

err:
        if (fp)
                fclose(fp);

        if (buf)
                free(buf);

        return NULL;
}

int file_write(const char *path, const void *data, size_t sz)
{
        FILE *fp = 0;
        int ret = 0;

        if (!path || !data || !sz)
                return -EINVAL;

        fp = fopen(path, "wb");
        if (!fp) {
                pr_err("fopen(): %s: %s\n", path, strerror(errno));
                return errno;
        }

        if (fwrite(data, sz, 1, fp) != 1) {
                pr_err("fwrite(): %s: %s\n", path, strerror((ret = ferror(fp))));
        }

        fclose(fp);

        return ret;
}
