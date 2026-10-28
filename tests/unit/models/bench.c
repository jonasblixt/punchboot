/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench.h"
#include "fakes/plat.h"

#define LOG_MAX          64
#define STUCK_LIMIT_US   (10ULL * 1000 * 1000)
#define BLOCKS_PER_CHUNK 128

struct violation {
    char section[32];
    char text[200];
    unsigned int repeats;
};

static struct violation log_entries[LOG_MAX];
static unsigned int n_violations;
static uint32_t last_tick;
static uint64_t now_us;
static uint64_t stuck_at_us;

void bench_reset(void)
{
    n_violations = 0;
    fake_us_tick = FAKE_US_TICK_DEFAULT;
    fake_us_tick_step = 1;
    last_tick = fake_us_tick;
    now_us = 0;
    stuck_at_us = STUCK_LIMIT_US;
}

void bench_violation(const char *section, const char *fmt, ...)
{
    va_list ap;
    char text[200];

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    /* The same violation again is counted, not printed */
    for (unsigned int i = 0; i < n_violations && i < LOG_MAX; i++) {
        if (strcmp(log_entries[i].section, section) == 0 &&
            strcmp(log_entries[i].text, text) == 0) {
            log_entries[i].repeats++;
            return;
        }
    }

    print_message("[ VIOLATION] %s: %s\n", section, text);

    if (n_violations < LOG_MAX) {
        snprintf(log_entries[n_violations].section, sizeof(log_entries[0].section), "%s", section);
        snprintf(log_entries[n_violations].text, sizeof(log_entries[0].text), "%s", text);
        log_entries[n_violations].repeats = 0;
    }
    n_violations++;
}

unsigned int bench_violations(void)
{
    return n_violations;
}

unsigned int bench_violations_of(const char *section)
{
    unsigned int n = 0;

    for (unsigned int i = 0; i < n_violations && i < LOG_MAX; i++) {
        if (strcmp(log_entries[i].section, section) == 0)
            n++;
    }
    return n;
}

void bench_dump(void)
{
    for (unsigned int i = 0; i < n_violations && i < LOG_MAX; i++)
        print_message("  %2u: %s: %s (x%u)\n",
                      i,
                      log_entries[i].section,
                      log_entries[i].text,
                      log_entries[i].repeats + 1);
    if (n_violations > LOG_MAX)
        print_message("  ... and %u more\n", n_violations - LOG_MAX);
}

uint64_t bench_now_us(void)
{
    now_us += (uint32_t)(fake_us_tick - last_tick);
    last_tick = fake_us_tick;
    return now_us;
}

void bench_advance_us(uint32_t us)
{
    fake_us_tick += us;
}

void bench_check_stuck(const char *what)
{
    if (bench_now_us() > stuck_at_us) {
        /* Once is enough, teardown may poll again */
        stuck_at_us = UINT64_MAX;
        fail_msg("Stuck: %s for more than %llu s of simulated time",
                 what,
                 (unsigned long long)(STUCK_LIMIT_US / 1000000));
    }
}

void bench_store_init(struct bench_store *s, uint64_t blocks, size_t block_size)
{
    s->blocks = blocks;
    s->block_size = block_size;
    s->n_chunks = (blocks + BLOCKS_PER_CHUNK - 1) / BLOCKS_PER_CHUNK;
    s->chunks = calloc(s->n_chunks, sizeof(*s->chunks));
    assert_non_null(s->chunks);
}

void bench_store_free(struct bench_store *s)
{
    for (size_t i = 0; i < s->n_chunks; i++)
        free(s->chunks[i]);
    free(s->chunks);
    memset(s, 0, sizeof(*s));
}

void bench_store_read(const struct bench_store *s, uint64_t block, void *buf)
{
    const uint8_t *chunk = s->chunks[block / BLOCKS_PER_CHUNK];

    assert_true(block < s->blocks);
    if (chunk == NULL)
        memset(buf, 0, s->block_size);
    else
        memcpy(buf, &chunk[(block % BLOCKS_PER_CHUNK) * s->block_size], s->block_size);
}

void bench_store_write(struct bench_store *s, uint64_t block, const void *buf)
{
    uint8_t **chunk = &s->chunks[block / BLOCKS_PER_CHUNK];

    assert_true(block < s->blocks);
    if (*chunk == NULL) {
        *chunk = calloc(BLOCKS_PER_CHUNK, s->block_size);
        assert_non_null(*chunk);
    }
    memcpy(&(*chunk)[(block % BLOCKS_PER_CHUNK) * s->block_size], buf, s->block_size);
}

/* Generator x^7 + x^3 + 1, MSB first */
uint8_t bench_crc7(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;

    for (size_t n = 0; n < len; n++) {
        for (int i = 7; i >= 0; i--) {
            unsigned int in = (data[n] >> i) & 1;
            unsigned int msb = (crc >> 6) & 1;

            crc = (crc << 1) & 0x7f;
            if (in ^ msb)
                crc ^= 0x09;
        }
    }
    return crc;
}

void bench_set_bits(uint32_t r[4], unsigned int hi, unsigned int lo, uint32_t value)
{
    for (unsigned int bit = lo; bit <= hi; bit++) {
        uint32_t mask = 1u << (bit % 32);

        if (value & (1u << (bit - lo)))
            r[bit / 32] |= mask;
        else
            r[bit / 32] &= ~mask;
    }
}

uint32_t bench_get_bits(const uint32_t r[4], unsigned int hi, unsigned int lo)
{
    uint32_t value = 0;

    for (unsigned int bit = lo; bit <= hi; bit++) {
        if (r[bit / 32] & (1u << (bit % 32)))
            value |= 1u << (bit - lo);
    }
    return value;
}

void bench_reg128_seal(uint32_t r[4])
{
    uint8_t bytes[15];

    /* Bits [127:8], MSB first */
    for (int i = 0; i < 15; i++)
        bytes[i] = bench_get_bits(r, 127 - 8 * i, 120 - 8 * i);

    bench_set_bits(r, 7, 1, bench_crc7(bytes, sizeof(bytes)));
    bench_set_bits(r, 0, 0, 1);
}
