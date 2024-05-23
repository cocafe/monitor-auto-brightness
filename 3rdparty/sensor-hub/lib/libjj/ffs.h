#ifndef __JJ_FFS_H__
#define __JJ_FFS_H__

#include "bits.h"
#include "compiler.h"

#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG && (nbits) > 0)

/**
 * __ffs - find first bit in word.
 * @word: The word to search
 *
 * Undefined if no bit exists, so code should check against 0 first.
 */
static __always_inline unsigned long __ffs(unsigned long word)
{
        int num = 0;

#if BITS_PER_LONG == 64
        if ((word & 0xffffffff) == 0) {
		num += 32;
		word >>= 32;
	}
#endif
        if ((word & 0xffff) == 0) {
                num += 16;
                word >>= 16;
        }
        if ((word & 0xff) == 0) {
                num += 8;
                word >>= 8;
        }
        if ((word & 0xf) == 0) {
                num += 4;
                word >>= 4;
        }
        if ((word & 0x3) == 0) {
                num += 2;
                word >>= 2;
        }
        if ((word & 0x1) == 0)
                num += 1;
        return num;
}

/*
 * Find the first set bit in a memory region.
 */
static inline unsigned long _find_first_bit(const unsigned long *addr, unsigned long bits)
{
        unsigned long idx;

        for (idx = 0; idx * BITS_PER_LONG < bits; idx++) {
                if (addr[idx])
                        return min(idx * BITS_PER_LONG + __ffs(addr[idx]), bits);
        }

        return bits;
}

/**
 * find_first_bit - find the first set bit in a memory region
 * @addr: The address to start the search at
 * @bits: The maximum number of bits to search
 *
 * @return the bit number of the first set bit.
 * If no bits are set, returns @size.
 */
static inline unsigned long find_first_bit(const unsigned long *addr, unsigned long bits)
{
        if (small_const_nbits(bits)) {
                unsigned long val = *addr & GENMASK(bits - 1, 0);

                return val ? __ffs(val) : bits;
        }

        return _find_first_bit(addr, bits);
}

/*
 * ffz - find first zero in word.
 * @word: The word to search
 *
 * Undefined if no zero exists, so code should check against ~0UL first.
 */
#define ffz(x)  __ffs(~(x))

/*
 * Find the first cleared bit in a memory region.
 */
static inline unsigned long _find_first_zero_bit(const unsigned long *addr, unsigned long bits)
{
        unsigned long idx;

        for (idx = 0; idx * BITS_PER_LONG < bits; idx++) {
                if (addr[idx] != ~0UL)
                        return min(idx * BITS_PER_LONG + ffz(addr[idx]), bits);
        }

        return bits;
}

/**
 * find_first_zero_bit - find the first cleared bit in a memory region
 * @addr: The address to start the search at
 * @bits: The maximum number of bits to search
 *
 * @return the bit number of the first cleared bit.
 * If no bits are zero, returns @size.
 */
static inline unsigned long find_first_zero_bit(const unsigned long *addr, unsigned long bits)
{
        if (small_const_nbits(bits)) {
                unsigned long val = *addr | ~GENMASK(bits - 1, 0);

                return val == ~0UL ? bits : ffz(val);
        }

        return _find_first_zero_bit(addr, bits);
}

#if BITS_PER_LONG == 32
static inline unsigned long find_first_bit_u64(const uint64_t *addr)
{
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        struct {
                uint32_t hi;
                uint32_t lo;
        } *u64_rep = (void *)addr;
#else /* __ORDER_LITTLE_ENDIAN__ */
        struct {
                uint32_t lo;
                uint32_t hi;
        } *u64_rep = (void *)addr;
#endif

        unsigned long ret;

        ret = find_first_bit((void *)&u64_rep->lo, SIZE_TO_BITS(uint32_t));
        if (ret == 32)
                ret += find_first_bit((void *)&u64_rep->hi, SIZE_TO_BITS(uint32_t));

        return ret;
}

static inline unsigned long find_first_zero_bit_u64(const uint64_t *addr)
{
#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
        struct {
                uint32_t hi;
                uint32_t lo;
        } *u64_rep = (void *)addr;
#else /* __ORDER_LITTLE_ENDIAN__ */
        struct {
                uint32_t lo;
                uint32_t hi;
        } *u64_rep = (void *)addr;
#endif

        unsigned long ret;

        ret = find_first_zero_bit((void *)&u64_rep->lo, SIZE_TO_BITS(uint32_t));
        if (ret == 32)
                ret += find_first_zero_bit((void *)&u64_rep->hi, SIZE_TO_BITS(uint32_t));

        return ret;
}
#else
#define find_first_bit_u64 find_first_bit
#define find_first_zero_bit_u64 find_first_zero_bit
#endif // BITS_PER_LONG == 32

#endif // __JJ_FFS_H__