#ifndef __JJ_UUID_H__
#define __JJ_UUID_H__

#include <stdint.h>

#include "endian_wrap.h"

// mingw uses msvc GUID definitions
#if !defined(__MINGW64__) && !defined(__MINGW32__)
#include <uuid/uuid.h>
#endif

struct uuid {
        uint32_t time_low;              // be
        uint16_t time_mid;              // be
        uint16_t time_hi_and_ver;       // be
        uint8_t  clock_seq_hi_and_resvd;
        uint8_t  clock_seq_low;
        uint8_t  node[6];
};

#define UUID_FMT        "%8.8x-%4.4x-%4.4x-%2.2x%2.2x-%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x"
#define UUID_ARG(x)     be32toh(x.time_low), be16toh(x.time_mid), be16toh(x.time_hi_and_ver), \
                        x.clock_seq_hi_and_resvd, x.clock_seq_low, \
                        x.node[0], x.node[1], x.node[2], \
                        x.node[3], x.node[4], x.node[5]
#define UUID_PTR(x)     be32toh(x->time_low), be16toh(x->time_mid), be16toh(x->time_hi_and_ver), \
                        x->clock_seq_hi_and_resvd, x->clock_seq_low, \
                        x->node[0], x->node[1], x->node[2], \
                        x->node[3], x->node[4], x->node[5]
#define UUID_ANY_PTR(x) UUID_PTR((struct uuid *) { (void *)(x) })

#endif // __JJ_UUID_H__