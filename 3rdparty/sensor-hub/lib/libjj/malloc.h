#ifndef __JJ_MALLOC_H__
#define __JJ_MALLOC_H__

#include <stdint.h>

#if defined (__MSVCRT__) || defined (_MSC_VER)
void heap_init(void);
void *halloc(size_t sz);
void hfree(void *ptr);
#else
static inline void heap_init(void) { };
static inline void *halloc(size_t sz) { return malloc(sz); }
static inline void hfree(void *ptr) { free(ptr); }
#endif

void *realloc_safe(void *old_ptr, size_t old_sz, size_t new_sz);

#endif // __JJ_MALLOC_H__