/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Shared parts of the device models: the violation log, simulated time and
 * sparse block storage.
 *
 * A violation is something the host did that the standard (or the reference
 * manual, for controller models) does not allow. Each one names the section
 * it breaks, e.g. "JESD84-B51 6.6.2.2". The models keep running after a
 * violation and do what the device would do, so a test sees both the rule
 * that was broken and its effect.
 */

#ifndef TESTS_UNIT_MODELS_BENCH_H
#define TESTS_UNIT_MODELS_BENCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JESD84 "JESD84-B51 "
#define SD_PLS "SD PLS "

/* Clears the log, starts the simulated clock (1 us per plat_get_us_tick()) */
void bench_reset(void);

void bench_violation(const char *section, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

unsigned int bench_violations(void);

/* Number of different violations logged for 'section' */
unsigned int bench_violations_of(const char *section);

/* Prints the log */
void bench_dump(void);

/* Fails the running test if a violation was logged */
#define assert_bench_clean()                                                     \
    do {                                                                         \
        if (bench_violations() != 0) {                                           \
            bench_dump();                                                        \
            fail_msg("%u standard violation(s), see above", bench_violations()); \
        }                                                                        \
    } while (0)

/* Simulated time: advances with plat_get_us_tick() and bench_advance_us() */
uint64_t bench_now_us(void);
void bench_advance_us(uint32_t us);

/*
 * Fails the running test when the simulated time passes the limit set by
 * bench_reset() (10 s): a driver polls for something that never happens.
 */
void bench_check_stuck(const char *what);

/* Sparse block storage, unwritten blocks read as 0 */
struct bench_store {
    uint64_t blocks;
    size_t block_size;
    uint8_t **chunks;
    size_t n_chunks;
};

void bench_store_init(struct bench_store *s, uint64_t blocks, size_t block_size);
void bench_store_free(struct bench_store *s);
void bench_store_read(const struct bench_store *s, uint64_t block, void *buf);
void bench_store_write(struct bench_store *s, uint64_t block, const void *buf);

/* CRC7 of the MMC/SD protocol, as stored in bits [7:1] of CID and CSD */
uint8_t bench_crc7(const uint8_t *data, size_t len);

/* Bits [hi:lo] of a 128-bit register held in r[0] = [31:0] .. r[3] = [127:96] */
void bench_set_bits(uint32_t r[4], unsigned int hi, unsigned int lo, uint32_t value);
uint32_t bench_get_bits(const uint32_t r[4], unsigned int hi, unsigned int lo);

/* Sets the CRC7 of a CID or CSD in r[] (bits [7:1]) and the end bit */
void bench_reg128_seal(uint32_t r[4]);

#endif // TESTS_UNIT_MODELS_BENCH_H
