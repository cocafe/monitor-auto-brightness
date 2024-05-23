#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined (__MSVCRT__) || defined (_MSC_VER)
#include <heapapi.h>
#endif

#include "malloc.h"

#if defined (__MSVCRT__) || defined (_MSC_VER)
static HANDLE hHeap;

void heap_init(void)
{
        hHeap = GetProcessHeap();
}

void *halloc(size_t sz)
{
        return HeapAlloc(hHeap, HEAP_ZERO_MEMORY, sz);
}

void hfree(void *ptr)
{
        HeapFree(hHeap, 0, ptr);
}
#endif

// posix realloc() does not clear new area
void *realloc_safe(void *old_ptr, size_t old_sz, size_t new_sz)
{
        void *new_ptr;

        if (!old_ptr || !new_sz)
                return old_ptr;

        new_ptr = calloc(1, new_sz);
        if (!new_ptr)
                return NULL;

        if (new_sz >= old_sz)
                memcpy(new_ptr, old_ptr, old_sz);
        else
                memcpy(new_ptr, old_ptr, new_sz);

        free(old_ptr);

        return new_ptr;
}
