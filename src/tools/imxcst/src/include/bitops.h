/*
 *
 *     Copyright 2017-2019 NXP
 *
 *     Redistribution and use in source and binary forms, with or without modification,
 *     are permitted provided that the following conditions are met:
 *
 *     o Redistributions of source code must retain the above copyright notice, this list
 *       of conditions and the following disclaimer.
 *
 *     o Redistributions in binary form must reproduce the above copyright notice, this
 *       list of conditions and the following disclaimer in the documentation and/or
 *       other materials provided with the distribution.
 *
 *     o Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *     ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *     WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 *     ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *     (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *     ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *     (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *     SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef BITOPS_H
#define BITOPS_H

#include <stdint.h>

#define BIT(x)  (1UL << (x))

/* Generate a bitmask starting at index s, ending at index e */
#define BIT_MASK(e, s) ((((1UL) << (e - s + 1)) - 1) << s)

static inline uint8_t bf_get_uint8(uint32_t w, uint32_t m, uint8_t s)
{
    return (uint8_t) ((w & m) >> s);
}

static inline uint16_t bf_get_uint16(uint32_t w, uint32_t m, uint8_t s)
{
    return (uint16_t) ((w & m) >> s);
}

static inline uint32_t bf_get_uint32(uint32_t w, uint32_t m, uint8_t s)
{
    return (uint32_t) ((w & m) >> s);
}

static inline uint32_t bf_popcount(uint32_t x)
{
    return (uint32_t) (__builtin_popcount(x));
}

static inline uint32_t bf_ffs(uint32_t x)
{
    return (uint32_t) (__builtin_ffs(x));
}
#endif /* BITOPS_H */
