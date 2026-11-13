/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * See bio_conformance.h.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/bio.h>
#include <pb/pb.h>
#include <string.h>

#include "bio_conformance.h"

/* A length that isn't whole blocks of any block size, and more than a
 * driver's chunk in the test programs (16 KiB at most) */
#define LONG_BYTES (40 * 1024 + 100)
#define GUARD      0xee

bio_dev_t bio_conformance_dev = -1;
void (*bio_conformance_after)(void);

/* Static, and on a cache line: a controller model takes 32 bit addresses,
 * and a driver's device writes into 'in' (drivers.md 4.3) */
static uint8_t out[BIO_CONFORMANCE_MIN_BYTES] __aligned(64);
static uint8_t in[BIO_CONFORMANCE_MIN_BYTES] __aligned(64);

static bio_dev_t dev;
static size_t bs; /* The device's block size */
static lba_t last; /* Its last block */

static void begin(void)
{
    dev = bio_conformance_dev;
    assert_true(bio_valid(dev));
    assert_true(bio_block_size(dev) > 0);
    assert_true(bio_size(dev) >= BIO_CONFORMANCE_MIN_BYTES);
    bs = (size_t)bio_block_size(dev);
    last = (lba_t)(bio_size(dev) / (int64_t)bs - 1);
    assert_true(8 * bs <= sizeof(out));
}

static void end(void)
{
    if (bio_conformance_after != NULL)
        bio_conformance_after();
}

/* Never 0, so that padding can be told from data */
static void fill(uint8_t *buf, size_t len, unsigned int seed)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)((i * 7 + seed + (i >> 9)) | 1);
}

static void assert_all(const uint8_t *buf, size_t len, uint8_t value)
{
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != value)
            fail_msg("Byte %zu of %zu is 0x%02x, not 0x%02x", i, len, buf[i], value);
    }
}

/* bio.md 3.1: exactly 'length' bytes are written to the buffer, never more,
 * also when the length isn't whole blocks */
void bio_3_1_a_read_that_isnt_whole_blocks_is_trimmed(void **state)
{
    size_t len;
    (void)state;

    begin();
    fill(out, 4 * bs, 1);
    assert_int_equal(bio_write(dev, 0, 4 * bs, out), PB_OK);

    /* Less than a block */
    len = bs / 2 + 3;
    memset(in, GUARD, 4 * bs);
    assert_int_equal(bio_read(dev, 1, len, in), PB_OK);
    assert_memory_equal(in, out + bs, len);
    assert_all(in + len, 4 * bs - len, GUARD);

    /* Whole blocks and a part of the next one */
    len = 2 * bs + 1;
    memset(in, GUARD, 4 * bs);
    assert_int_equal(bio_read(dev, 1, len, in), PB_OK);
    assert_memory_equal(in, out + bs, len);
    assert_all(in + len, 4 * bs - len, GUARD);

    /* One byte, of the device's last block */
    assert_int_equal(bio_write(dev, last, bs, out), PB_OK);
    memset(in, GUARD, 4 * bs);
    assert_int_equal(bio_read(dev, last, 1, in), PB_OK);
    assert_int_equal(in[0], out[0]);
    assert_all(in + 1, 4 * bs - 1, GUARD);
    end();
}

/* bio.md 3.2: the last block is the bytes given, then zeros; nothing else is
 * written */
void bio_3_2_a_write_that_isnt_whole_blocks_is_padded_with_zeros(void **state)
{
    size_t len;
    (void)state;

    begin();
    /* What the blocks held before */
    memset(out, 0xff, 4 * bs);
    assert_int_equal(bio_write(dev, 0, 4 * bs, out), PB_OK);
    assert_int_equal(bio_write(dev, last, bs, out), PB_OK);

    /* A whole block and a part of the next one */
    len = bs + bs / 2 - 1;
    fill(out, 2 * bs, 2);
    assert_int_equal(bio_write(dev, 1, len, out), PB_OK);

    assert_int_equal(bio_read(dev, 0, 4 * bs, in), PB_OK);
    assert_all(in, bs, 0xff);
    assert_memory_equal(in + bs, out, len);
    assert_all(in + bs + len, 2 * bs - len, 0);
    assert_all(in + 3 * bs, bs, 0xff);

    /* One byte, to the device's last block */
    assert_int_equal(bio_write(dev, last, 1, out), PB_OK);
    assert_int_equal(bio_read(dev, last, bs, in), PB_OK);
    assert_int_equal(in[0], out[0]);
    assert_all(in + 1, bs - 1, 0);
    end();
}

/* bio.md 3.1, 3.2: a length of 0 is PB_OK, and nothing is read or written */
void bio_3_a_length_of_0_does_nothing(void **state)
{
    (void)state;

    begin();
    fill(out, bs, 3);
    assert_int_equal(bio_write(dev, 2, bs, out), PB_OK);

    memset(in, GUARD, bs);
    assert_int_equal(bio_read(dev, 2, 0, in), PB_OK);
    assert_all(in, bs, GUARD);
    assert_int_equal(bio_write(dev, 2, 0, in), PB_OK);

    assert_int_equal(bio_read(dev, 2, bs, in), PB_OK);
    assert_memory_equal(in, out, bs);
    end();
}

/* bio.md 3: a range outside the device is -PB_ERR_PARAM, and a block that is
 * only partly asked for counts */
void bio_3_a_range_outside_the_device_is_refused(void **state)
{
    (void)state;

    begin();
    fill(out, 2 * bs, 4);
    assert_int_equal(bio_write(dev, last, bs, out), PB_OK);

    assert_int_equal(bio_read(dev, last + 1, bs, in), -PB_ERR_PARAM);
    assert_int_equal(bio_write(dev, last + 1, bs, out + bs), -PB_ERR_PARAM);
    assert_int_equal(bio_read(dev, last, bs + 1, in), -PB_ERR_PARAM);
    assert_int_equal(bio_write(dev, last, bs + 1, out + bs), -PB_ERR_PARAM);
    assert_int_equal(bio_read(dev, last + 1, 1, in), -PB_ERR_PARAM);
    assert_int_equal(bio_write(dev, last + 1, 1, out + bs), -PB_ERR_PARAM);

    /* Nothing of them was written */
    assert_int_equal(bio_read(dev, last, bs, in), PB_OK);
    assert_memory_equal(in, out, bs);
    end();
}

/* bio.md 3.1: no limit on the length but the device's size, the driver
 * splits what its controller can't take in one piece. Here with a length
 * that isn't whole blocks */
void bio_3_any_length_in_one_call(void **state)
{
    size_t whole;
    (void)state;

    begin();
    whole = (LONG_BYTES + bs - 1) / bs * bs;
    assert_true(whole + bs <= sizeof(out));

    /* What the blocks held before */
    memset(out, 0xff, whole + bs);
    assert_int_equal(bio_write(dev, 0, whole + bs, out), PB_OK);

    fill(out, LONG_BYTES, 5);
    assert_int_equal(bio_write(dev, 0, LONG_BYTES, out), PB_OK);

    memset(in, GUARD, sizeof(in));
    assert_int_equal(bio_read(dev, 0, LONG_BYTES, in), PB_OK);
    assert_memory_equal(in, out, LONG_BYTES);
    assert_all(in + LONG_BYTES, sizeof(in) - LONG_BYTES, GUARD);

    /* The medium: the bytes, zeros to the end of their last block, and the
     * block after it as it was */
    assert_int_equal(bio_read(dev, 0, whole + bs, in), PB_OK);
    assert_memory_equal(in, out, LONG_BYTES);
    assert_all(in + LONG_BYTES, whole - LONG_BYTES, 0);
    assert_all(in + whole, bs, 0xff);
    end();
}
