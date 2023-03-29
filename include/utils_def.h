/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
/*
 * Copyright (c) 2016-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INCLUDE_PB_UTILS_DEF_H
#define INCLUDE_PB_UTILS_DEF_H

/* Compute the number of elements in the given array */
#define ARRAY_SIZE(a)                \
    (sizeof(a) / sizeof((a)[0]))

#define IS_POWER_OF_TWO(x) (((x) & ((x) - 1)) == 0)

#define SIZE_FROM_LOG2_WORDS(n)        (4 << (n))

#define BIT_32(nr)            (U(1) << (nr))
#define BIT_64(nr)            (ULL(1) << (nr))

#ifdef AARCH32
#define BIT                BIT_32
#else
#define BIT                BIT_64
#endif

/*
 * Create a contiguous bitmask starting at bit position @l and ending at
 * position @h. For example
 * GENMASK_64(39, 21) gives us the 64bit vector 0x000000ffffe00000.
 */
#if defined(__LINKER__) || defined(__ASSEMBLY__)
#define GENMASK_32(h, l) \
    (((0xFFFFFFFF) << (l)) & (0xFFFFFFFF >> (32 - 1 - (h))))

#define GENMASK_64(h, l) \
    ((~0 << (l)) & (~0 >> (64 - 1 - (h))))
#else
#define GENMASK_32(h, l) \
    (((~UINT32_C(0)) << (l)) & (~UINT32_C(0) >> (32 - 1 - (h))))

#define GENMASK_64(h, l) \
    (((~UINT64_C(0)) << (l)) & (~UINT64_C(0) >> (64 - 1 - (h))))
#endif

#ifdef AARCH32
#define GENMASK                GENMASK_32
#else
#define GENMASK                GENMASK_64
#endif

/*
 * This variant of div_round_up can be used in macro definition but should not
 * be used in C code as the `div` parameter is evaluated twice.
 */
#define DIV_ROUND_UP_2EVAL(n, d)    (((n) + (d) - 1) / (d))

#define div_round_up(val, div) __extension__ ({    \
    __typeof__(div) _div = (div);        \
    ((val) + _div - (__typeof__(div)) 1) / _div;        \
})

#define MIN(x, y) __extension__ ({    \
    __typeof__(x) _x = (x);        \
    __typeof__(y) _y = (y);        \
    (void)(&_x == &_y);        \
    _x < _y ? _x : _y;        \
})

#define MAX(x, y) __extension__ ({    \
    __typeof__(x) _x = (x);        \
    __typeof__(y) _y = (y);        \
    (void)(&_x == &_y);        \
    _x > _y ? _x : _y;        \
})

/*
 * The round_up() macro rounds up a value to the given boundary in a
 * type-agnostic yet type-safe manner. The boundary must be a power of two.
 * In other words, it computes the smallest multiple of boundary which is
 * greater than or equal to value.
 *
 * round_down() is similar but rounds the value down instead.
 */
#define round_boundary(value, boundary)        \
    ((__typeof__(value))((boundary) - 1))

#define round_up(value, boundary)        \
    ((((value) - 1) | round_boundary(value, boundary)) + 1)

#define round_down(value, boundary)        \
    ((value) & ~round_boundary(value, boundary))

/*
 * Evaluates to 1 if (ptr + inc) overflows, 0 otherwise.
 * Both arguments must be unsigned pointer values (i.e. uintptr_t).
 */
#define check_uptr_overflow(_ptr, _inc)        \
    ((_ptr) > (UINTPTR_MAX - (_inc)))

/*
 * Evaluates to 1 if (u32 + inc) overflows, 0 otherwise.
 * Both arguments must be 32-bit unsigned integers (i.e. effectively uint32_t).
 */
#define check_u32_overflow(_u32, _inc) \
    ((_u32) > (UINT32_MAX - (_inc)))

/*
 * For those constants to be shared between C and other sources, apply a 'U',
 * 'UL', 'ULL', 'L' or 'LL' suffix to the argument only in C, to avoid
 * undefined or unintended behaviour.
 *
 * The GNU assembler and linker do not support these suffixes (it causes the
 * build process to fail) therefore the suffix is omitted when used in linker
 * scripts and assembler files.
*/
#if defined(__LINKER__) || defined(__ASSEMBLY__)
# define   U(_x)    (_x)
# define  UL(_x)    (_x)
# define ULL(_x)    (_x)
# define   L(_x)    (_x)
# define  LL(_x)    (_x)
#else
# define   U(_x)    (_x##U)
# define  UL(_x)    (_x##UL)
# define ULL(_x)    (_x##ULL)
# define   L(_x)    (_x##L)
# define  LL(_x)    (_x##LL)
#endif

/*
 * Import an assembly or linker symbol as a C expression with the specified
 * type
 */
#define IMPORT_SYM(type, sym, name) \
    extern char sym[];\
    static const __attribute__((unused)) type name = (type) sym;

/*
 * When the symbol is used to hold a pointer, its alignment can be asserted
 * with this macro. For example, if there is a linker symbol that is going to
 * be used as a 64-bit pointer, the value of the linker symbol must also be
 * aligned to 64 bit. This macro makes sure this is the case.
 */
#define ASSERT_SYM_PTR_ALIGN(sym) \
                assert(((size_t)(sym) % __alignof__(*(sym))) == 0)

#define COMPILER_BARRIER() __asm__ volatile ("" ::: "memory")

#define PB_CHECK_OVERLAP(__a, __sz, __region_start, __region_end) \
    (((__a) <= ((uintptr_t) (__region_end))) &&                 \
     ((__a) + (__sz) >= ((uintptr_t) (__region_start))))

#define membersof(array) (sizeof(array) / sizeof((array)[0]))


#define SZ_kB(x) ((size_t) (x) << 10)
#define SZ_MB(x) ((size_t) (x) << 20)
#define SZ_GB(x) ((size_t) (x) << 30)
#define MHz(x) (x * 1000000UL)

#endif  // INCLUDE_UTILS_DEF_H
