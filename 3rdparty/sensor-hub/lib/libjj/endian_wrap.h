#ifndef __JJ_ENDIAN_WRAP_H__
#define __JJ_ENDIAN_WRAP_H__

#if defined(__MINGW64__) || defined(__MINGW32__)
#include "endian_mingw.h"
#endif

#if defined(__CYGWIN__) || defined(__linux__) || defined(__unix__)
#include <endian.h>
#endif

#endif // __JJ_ENDIAN_WRAP_H__
