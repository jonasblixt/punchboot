/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The virtio block driver (src/drivers/virtio/virtio_block.c) against a
 * model of a legacy virtio-mmio block device behind the fake pb/mmio.h
 * (VIRTIO 1.1 4.2.4, 5.2): the queue is set up through QUEUE_NUM,
 * QUEUE_ALIGN and QUEUE_PFN, a write of QUEUE_NOTIFY has the device take the
 * requests in the available ring and put them in the used ring, and a write
 * of 0 to STATUS resets it. The configuration space is read straight from
 * memory, as the driver does, so it is the model's own buffer.
 *
 * Built without PIE: the driver hands the device 32-bit addresses. The DMA
 * and cache rule isn't checked here: the driver doesn't use pb/dma.h yet
 * (F-052), and the arch's cache functions are empty.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/virtio/virtio_block.h>
#include <pb/bio.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <string.h>

#include "../../src/drivers/virtio/virtio_mmio.h"
#include "../../src/drivers/virtio/virtio_queue.h"
#include "fakes/plat.h"

#define BLOCKS 64
#define BLOCK  512

#define UU (const unsigned char *)"\x3c\x51\x8e\x07\x6d\x2a\x4b\x90\xa1\x5f\x22\x9e\x0b\x74\xc3\x18"

/* The device's registers and, from VIRTIO_MMIO_CONFIG, its configuration */
static uint8_t dev_mem[0x200] __aligned(8);

static struct {
    uint32_t status;
    uint32_t queue_num;
    uint32_t queue_align;
    uint32_t queue_pfn;
    uint16_t last_avail;
    bool stuck; /* Takes no request, ever */
    unsigned int notifies;
    unsigned int resets;
    uint8_t disk[BLOCKS * BLOCK];
} m;

struct blk_req {
    uint32_t type;
    uint32_t reserved;
    uint32_t sector_low;
    uint32_t sector_hi;
};

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

static void *ptr(uint64_t addr)
{
    return (void *)(uintptr_t)addr;
}

/* The requests in the available ring: a request header, the data, a status
 * byte (VIRTIO 1.1 5.2.6) */
static void process(void)
{
    uint8_t *q = ptr((uint64_t)m.queue_pfn << 12);
    struct virtq_desc *desc = (struct virtq_desc *)q;
    struct virtq_avail *avail =
        (struct virtq_avail *)(q + VIRTIO_QUEUE_AVAIL_OFFSET(m.queue_num, m.queue_align));
    struct virtq_used *used =
        (struct virtq_used *)(q + VIRTIO_QUEUE_USED_OFFSET(m.queue_num, m.queue_align));

    assert_int_not_equal(m.queue_pfn, 0);
    assert_true(m.status & VIRTIO_STATUS_DRIVER_OK);

    while (m.last_avail != avail->idx) {
        uint16_t head = avail->ring[m.last_avail % m.queue_num];
        struct virtq_desc *d0 = &desc[head];
        struct virtq_desc *d1 = &desc[d0->next];
        struct virtq_desc *d2 = &desc[d1->next];
        struct blk_req *req = ptr(d0->addr);
        uint8_t *data = ptr(d1->addr);
        size_t offset = (size_t)req->sector_low * BLOCK;

        assert_true(offset + d1->len <= sizeof(m.disk));

        if (req->type == 0)
            memcpy(data, m.disk + offset, d1->len);
        else
            memcpy(m.disk + offset, data, d1->len);

        *(uint8_t *)ptr(d2->addr) = 0; /* VIRTIO_BLK_S_OK */
        used->ring[used->idx % m.queue_num].id = head;
        used->ring[used->idx % m.queue_num].len = d1->len + 1;
        used->idx++;
        m.last_avail++;
    }
}

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    assert_int_equal(width, 32);

    switch (addr - (uintptr_t)dev_mem) {
    case VIRTIO_MMIO_DEVICE_ID:
        return 2;
    case VIRTIO_MMIO_QUEUE_NUM_MAX:
        return 16;
    case VIRTIO_MMIO_STATUS:
        return m.status;
    default:
        fail_msg("virtio model: read of 0x%" PRIxPTR, addr - (uintptr_t)dev_mem);
    }

    return 0;
}

void fake_mmio_write(uintptr_t addr, uint64_t value64, unsigned int width)
{
    uint32_t value = (uint32_t)value64;

    assert_int_equal(width, 32);

    switch (addr - (uintptr_t)dev_mem) {
    case VIRTIO_MMIO_STATUS:
        if (value == 0) {
            /* A reset: the queue is gone, and what it held (4.2.4) */
            m.resets++;
            m.queue_pfn = 0;
            m.last_avail = 0;
        }
        m.status = value;
        break;
    case VIRTIO_MMIO_GUEST_PAGE_SIZE:
        assert_int_equal(value, 4096);
        break;
    case VIRTIO_MMIO_QUEUE_SEL:
        assert_int_equal(value, 0);
        break;
    case VIRTIO_MMIO_QUEUE_NUM:
        m.queue_num = value;
        break;
    case VIRTIO_MMIO_QUEUE_ALIGN:
        m.queue_align = value;
        break;
    case VIRTIO_MMIO_QUEUE_PFN:
        m.queue_pfn = value;
        break;
    case VIRTIO_MMIO_QUEUE_NOTIFY:
        m.notifies++;
        if (!m.stuck)
            process();
        break;
    default:
        fail_msg("virtio model: write of 0x%" PRIx32 " to 0x%" PRIxPTR,
                 value,
                 addr - (uintptr_t)dev_mem);
    }
}

static bio_dev_t dev;
static uint8_t out[4 * BLOCK] __aligned(PB_DMA_ALIGN);
static uint8_t in[4 * BLOCK] __aligned(PB_DMA_ALIGN);

static int setup(void **state)
{
    uint64_t capacity = BLOCKS;
    uint32_t blk_size = BLOCK;

    (void)state;
    memset(&m, 0, sizeof(m));
    memset(dev_mem, 0, sizeof(dev_mem));
    /* struct virtio_blk_config: capacity at 0, blk_size at 20 */
    memcpy(dev_mem + VIRTIO_MMIO_CONFIG, &capacity, sizeof(capacity));
    memcpy(dev_mem + VIRTIO_MMIO_CONFIG + 20, &blk_size, sizeof(blk_size));
    fake_us_tick = FAKE_US_TICK_DEFAULT;
    fake_us_tick_step = 10;
    bio_reset();

    dev = virtio_block_init((uintptr_t)dev_mem, UU);
    assert_true(dev >= 0);
    return 0;
}

static void fill(uint8_t *buf, size_t len, unsigned int seed)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)(i * 11 + seed);
}

static void test_read_write(void **state)
{
    (void)state;

    fill(out, sizeof(out), 1);
    assert_int_equal(bio_write(dev, 3, sizeof(out), out), PB_OK);
    assert_memory_equal(m.disk + 3 * BLOCK, out, sizeof(out));
    assert_int_equal(bio_read(dev, 3, sizeof(in), in), PB_OK);
    assert_memory_equal(in, out, sizeof(in));
}

/* drivers.md 3 (F-281): a device that never completes the request is
 * -PB_ERR_TIMEOUT within the driver's bound, where the driver waited for
 * ever. The device is reset, so nothing of the request completes later,
 * and the next request works */
static void test_completion_is_bounded(void **state)
{
    unsigned int start;
    (void)state;

    fill(out, sizeof(out), 2);
    assert_int_equal(bio_write(dev, 0, sizeof(out), out), PB_OK);

    m.stuck = true;
    start = fake_us_tick;
    assert_int_equal(bio_read(dev, 0, sizeof(in), in), -PB_ERR_TIMEOUT);
    assert_true(fake_us_tick - start >= 1000 * 1000);
    assert_true(fake_us_tick - start < 2 * 1000 * 1000);
    assert_int_equal(m.resets, 2); /* The init's and the timeout's */
    assert_true(m.status & VIRTIO_STATUS_DRIVER_OK);

    m.stuck = false;
    memset(in, 0, sizeof(in));
    assert_int_equal(bio_read(dev, 0, sizeof(in), in), PB_OK);
    assert_memory_equal(in, out, sizeof(in));
    fill(out, sizeof(out), 3);
    assert_int_equal(bio_write(dev, 8, sizeof(out), out), PB_OK);
    assert_memory_equal(m.disk + 8 * BLOCK, out, sizeof(out));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_read_write, setup),
        cmocka_unit_test_setup(test_completion_is_bounded, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
