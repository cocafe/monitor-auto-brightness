#ifndef __JJ_FILE_H__
#define __JJ_FILE_H__

char *file_read(const char *path);
int file_write(const char *path, const void *data, size_t sz);

#endif // __JJ_FILE_H__