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
bool bio_conformance_erases;
const struct bio_conformance_fault *bio_conformance_fault;

/* Static, and on a cache line: a controller model takes 32 bit addresses,
 * and a driver's device writes into 'in' (drivers.md 4.3) */
static uint8_t out[BIO_CONFORMANCE_MIN_BYTES] __aligned(64);
static uint8_t in[BIO_CONFORMANCE_MIN_BYTES] __aligned(64);

static bio_dev_t dev;
static size_t bs; /* The device's block size */
static lba_t last; /* Its last block */
static bool erase_first; /* BIO_FLAG_ERASE_BEFORE_WRITE */

static void begin(void)
{
    dev = bio_conformance_dev;
    assert_true(bio_valid(dev));
    assert_true(bio_block_size(dev) > 0);
    assert_true(bio_size(dev) >= BIO_CONFORMANCE_MIN_BYTES);
    bs = (size_t)bio_block_size(dev);
    last = (lba_t)(bio_size(dev) / (int64_t)bs - 1);
    assert_true(14 * bs <= sizeof(out));
    erase_first = (bio_get_flags(dev) & BIO_FLAG_ERASE_BEFORE_WRITE) != 0;

    /* A medium that only clears bits when it is written can be erased */
    if (erase_first)
        assert_true(bio_conformance_erases);
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

/* The blocks a length of bytes touches */
static size_t blocks_of(size_t len)
{
    return (len + bs - 1) / bs;
}

/* Erases what a write is about to store on a device with
 * BIO_FLAG_ERASE_BEFORE_WRITE: today the writer erases (bio.md 3.3) */
static void prepare(lba_t lba, size_t len)
{
    if (erase_first)
        assert_int_equal(bio_erase(dev, lba, blocks_of(len)), PB_OK);
}

/* A write that works, with the erase before it where the medium needs one */
static void put(lba_t lba, size_t len, const void *buf)
{
    prepare(lba, len);
    assert_int_equal(bio_write(dev, lba, len, buf), PB_OK);
}

/* The 'n' blocks from 'lba' read as 'expected' */
static void assert_blocks(lba_t lba, size_t n, const uint8_t *expected)
{
    assert_int_equal(bio_read(dev, lba, n * bs, in), PB_OK);
    assert_memory_equal(in, expected, n * bs);
}

/* drivers.md 3: what a driver answers for a medium that failed. There is no
 * partial success */
static void assert_failed(int rc)
{
    if (rc != -PB_ERR_IO && rc != -PB_ERR_TIMEOUT)
        fail_msg("The medium failed, the call answered %i, not -PB_ERR_IO or -PB_ERR_TIMEOUT", rc);
}

/* bio.md 3.1, 8: a read that the medium fails answers -PB_ERR_IO or
 * -PB_ERR_TIMEOUT (what it left in the buffer is unknown, F-173), and the
 * device reads after it. Whole blocks, the driver's own read, and half a
 * block, which the core reads into its block buffer */
void bio_3_1_a_read_that_fails(void **state)
{
    const struct bio_conformance_fault *fault = bio_conformance_fault;
    int rc;
    (void)state;

    begin();

    if (fault == NULL || fault->fail_reads == NULL) {
        print_message("Skipped: the test program can't make this medium's reads fail\n");
        skip();
    }

    fill(out, 4 * bs, 12);
    put(2, 4 * bs, out);

    fault->fail_reads();
    rc = bio_read(dev, 2, 4 * bs, in);
    fault->end();
    assert_failed(rc);
    assert_blocks(2, 4, out);

    fault->fail_reads();
    rc = bio_read(dev, 3, bs / 2, in);
    fault->end();
    assert_failed(rc);
    memset(in, 0, bs);
    assert_int_equal(bio_read(dev, 3, bs / 2, in), PB_OK);
    assert_memory_equal(in, out + bs, bs / 2);
    end();
}

/* bio.md 3.1: exactly 'length' bytes are written to the buffer, never more,
 * also when the length isn't whole blocks */
void bio_3_1_a_read_that_isnt_whole_blocks_is_trimmed(void **state)
{
    size_t len;
    (void)state;

    begin();
    fill(out, 4 * bs, 1);
    put(0, 4 * bs, out);

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
    put(last, bs, out);
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
    put(0, 4 * bs, out);
    put(last, bs, out);

    /* A whole block and a part of the next one */
    len = bs + bs / 2 - 1;
    fill(out, 2 * bs, 2);
    put(1, len, out);

    assert_int_equal(bio_read(dev, 0, 4 * bs, in), PB_OK);
    assert_all(in, bs, 0xff);
    assert_memory_equal(in + bs, out, len);
    assert_all(in + bs + len, 2 * bs - len, 0);
    assert_all(in + 3 * bs, bs, 0xff);

    /* One byte, to the device's last block */
    put(last, 1, out);
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
    put(2, bs, out);

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
    put(last, bs, out);

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
    put(0, whole + bs, out);

    fill(out, LONG_BYTES, 5);
    put(0, LONG_BYTES, out);

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

/* bio.md 3.2: a write that fails leaves any of its blocks written, and none
 * outside it; the caller sees the error, not PB_OK (drivers.md 3: there is
 * no partial success). The device is usable after it (drivers.md 3) */
void bio_3_2_a_write_that_fails_half_way(void **state)
{
    const struct bio_conformance_fault *fault = bio_conformance_fault;
    uint8_t *before = out; /* 8 blocks */
    uint8_t *data; /* 6 blocks */
    size_t len;
    int rc;
    (void)state;

    begin();
    data = out + 8 * bs;

    if (fault == NULL) {
        print_message("Skipped: the test program can't make this medium fail\n");
        skip();
    }

    fill(before, 8 * bs, 10);
    put(0, 8 * bs, before);
    fill(data, 6 * bs, 11);

    /* Blocks 1 to 6, the medium fails after two of them */
    prepare(1, 6 * bs);
    fault->fail_writes(1, 2);
    rc = bio_write(dev, 1, 6 * bs, data);
    fault->end();
    assert_failed(rc);
    assert_blocks(0, 1, before);
    assert_blocks(7, 1, before + 7 * bs);

    /* The same write again */
    put(1, 6 * bs, data);
    assert_blocks(0, 1, before);
    assert_blocks(1, 6, data);
    assert_blocks(7, 1, before + 7 * bs);

    /* A length that isn't whole blocks: the medium fails in the last block,
     * which the core writes from its block buffer (bio.md 3.2) */
    len = 2 * bs + bs / 2;
    prepare(1, len);
    fault->fail_writes(1, 2);
    rc = bio_write(dev, 1, len, before);
    fault->end();
    assert_failed(rc);
    assert_blocks(0, 1, before);
    assert_blocks(4, 3, data + 3 * bs);
    assert_blocks(7, 1, before + 7 * bs);

    put(1, len, before);
    assert_int_equal(bio_read(dev, 1, len, in), PB_OK);
    assert_memory_equal(in, before, len);
    assert_blocks(4, 3, data + 3 * bs);
    end();
}

/* bio.md 3.3: exactly the blocks named are erased. What an erased block
 * reads as isn't promised; that it is erased is: a device with
 * BIO_FLAG_ERASE_BEFORE_WRITE stores what is written to it next. A device
 * without an erase answers -PB_ERR_NOT_SUPPORTED */
void bio_3_3_an_erase_erases_exactly_the_blocks(void **state)
{
    uint8_t *before = out; /* 6 blocks */
    uint8_t *data; /* 2 blocks */
    (void)state;

    begin();
    data = out + 6 * bs;
    fill(before, 6 * bs, 12);
    put(0, 6 * bs, before);

    if (!bio_conformance_erases) {
        assert_int_equal(bio_erase(dev, 2, 2), -PB_ERR_NOT_SUPPORTED);
        assert_int_equal(bio_erase(dev, last, 1), -PB_ERR_NOT_SUPPORTED);
        assert_blocks(0, 6, before);
        end();
        return;
    }

    assert_int_equal(bio_erase(dev, 2, 2), PB_OK);
    assert_blocks(0, 2, before);
    assert_blocks(4, 2, before + 4 * bs);

    fill(data, 2 * bs, 13);
    assert_int_equal(bio_write(dev, 2, 2 * bs, data), PB_OK);
    assert_blocks(0, 2, before);
    assert_blocks(2, 2, data);
    assert_blocks(4, 2, before + 4 * bs);

    /* The device's last block, and not the one before it */
    put(last - 1, 2 * bs, before);
    assert_int_equal(bio_erase(dev, last, 1), PB_OK);
    assert_blocks(last - 1, 1, before);
    assert_int_equal(bio_write(dev, last, bs, data), PB_OK);
    assert_blocks(last - 1, 1, before);
    assert_blocks(last, 1, data);
    end();
}

/* bio.md 3, 3.3: a count of 0 is PB_OK, a range outside the device is
 * -PB_ERR_PARAM, and neither erases anything */
void bio_3_3_an_erase_of_0_blocks_or_outside_the_device(void **state)
{
    (void)state;

    begin();

    if (!bio_conformance_erases) {
        print_message("Skipped: the device has no erase (bio.md 3.3)\n");
        skip();
    }

    fill(out, 2 * bs, 14);
    put(0, bs, out);
    put(last, bs, out + bs);

    assert_int_equal(bio_erase(dev, 0, 0), PB_OK);
    assert_int_equal(bio_erase(dev, last, 0), PB_OK);
    assert_int_equal(bio_erase(dev, last, 2), -PB_ERR_PARAM);
    assert_int_equal(bio_erase(dev, last + 1, 1), -PB_ERR_PARAM);
    assert_int_equal(bio_erase(dev, 0, (size_t)last + 2), -PB_ERR_PARAM);
    assert_int_equal(bio_erase(dev, 0, SIZE_MAX), -PB_ERR_PARAM);

    assert_blocks(0, 1, out);
    assert_blocks(last, 1, out + bs);
    end();
}

/* bio.md 3.4: write_done after a complete write is PB_OK whether the device
 * has the callback or not, what was written stays, and the device reads
 * and writes after it, for the next stream too */
void bio_3_4_write_done_keeps_what_was_written(void **state)
{
    uint8_t *first = out; /* 4 blocks */
    uint8_t *second; /* 2 blocks */
    (void)state;

    begin();
    second = out + 4 * bs;
    fill(first, 4 * bs, 15);
    put(0, 4 * bs, first);
    assert_int_equal(bio_write_done(dev), PB_OK);
    assert_blocks(0, 4, first);

    fill(second, 2 * bs, 16);
    put(1, 2 * bs, second);
    assert_int_equal(bio_write_done(dev), PB_OK);
    assert_blocks(0, 1, first);
    assert_blocks(1, 2, second);
    assert_blocks(3, 1, first + 3 * bs);
    end();
}
