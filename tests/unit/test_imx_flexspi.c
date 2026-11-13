/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The FlexSPI NOR driver (src/drivers/memc/imx_flexspi.c) against bio.md 3,
 * with a NOR flash modelled behind the controller's registers (fakes/regs.h).
 *
 * The controller, as the RT1060 RM describes it: an IP command is the
 * sequence IPCR1.ISEQID names, at IPCR0, of IPCR1.IDATSZ bytes (16 bits),
 * started by IPCMD.TRG. Read data comes through RFDR, a watermark (8 bytes,
 * what the driver sets) at a time, and INTR.IPRXWA (write 1) pops it; write
 * data goes into TFDR and INTR.IPTXWE (write 1) pushes it. INTR's flags are
 * write 1 to clear. MCR0.SWRESET clears itself.
 *
 * The flash, as a SPI NOR (the EVK's IS25WP064D): a page program or an
 * erase needs write enable first and clears it, is ignored without it,
 * keeps WIP set in the status register for a while, and programs only clear
 * bits. A program wraps inside its page, an erase takes the aligned block
 * its address is in. A command other than a status read while WIP is set is
 * a violation of the model, as is anything the driver does that the model
 * doesn't know.
 *
 * The sequence ids are the test's own: the model knows a sequence by its id,
 * not by the LUT's content.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/memc/imx_flexspi.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "fakes/plat.h"
#include "fakes/regs.h"

#define BASE          0x402a8000u

/* The controller's registers (RT1060 RM, FlexSPI's memory map) */
#define MCR0          0x000
#define INTR          0x014
#define IPCR0         0x0a0
#define IPCR1         0x0a4
#define IPCMD         0x0b0
#define IPRXFCR       0x0b8
#define IPTXFCR       0x0bc
#define STS0          0x0e0
#define IPRXFSTS      0x0f0
#define RFDR(n)       (0x100 + 4 * (n))
#define TFDR(n)       (0x180 + 4 * (n))
#define FLSHCR2(n)    (0x080 + 4 * (n))

#define MCR0_SWRESET  BIT(0)
#define INTR_IPCMDERR BIT(3)
#define INTR_IPRXWA   BIT(5)
#define INTR_IPTXWE   BIT(6)
#define STS0_IDLE     (BIT(0) | BIT(1))
#define WATERMARK     8 /* Bytes, what the driver sets */
#define RX_ENTRIES    16 /* The RX FIFO's 64 bit entries */

/* The flash */
#define CAPACITY      (256 * 1024)
#define BLOCK         4096
#define PAGE          256
#define WIP           BIT(0)
#define WEL           BIT(1)

enum seq {
    SEQ_READ = 0,
    SEQ_READ_STATUS = 1,
    SEQ_READ_ID = 2,
    SEQ_WREN = 3,
    SEQ_WRDI = 4,
    SEQ_PP = 5,
    SEQ_ERASE_4K = 6,
    SEQ_ERASE_32K = 7,
    SEQ_ERASE_64K = 8,
};

static struct {
    uint8_t mem[CAPACITY];
    size_t page; /* The program's page, PAGE unless a test changes it */
    bool wel;
    unsigned int busy; /* Status reads that still see WIP */
    unsigned int busy_after; /* busy after a program or an erase */
    bool stuck_busy;
    int fail_seq; /* A sequence that ends in IPCMDERR, -1 for none */

    /* The controller */
    uint32_t intr;
    bool swreset_stuck;
    bool tx_stuck; /* IPTXWE never set */
    bool rx_stuck; /* No read data, ever */

    /* The command running */
    int seq;
    uint32_t addr;
    uint8_t rx[64 * 1024];
    size_t rx_len, rx_pos;
    uint8_t tx[64 * 1024];
    size_t tx_len, tx_pos;
    bool writing;

    unsigned int programs, erases, ignored, violations;
} nor;

static void violation(const char *what)
{
    print_message("NOR model: %s\n", what);
    nor.violations++;
}

static void update_intr(void)
{
    uint32_t intr = nor.intr & ~(INTR_IPRXWA | INTR_IPTXWE);

    if (!nor.rx_stuck && nor.rx_len - nor.rx_pos >= WATERMARK)
        intr |= INTR_IPRXWA;

    if (!nor.tx_stuck)
        intr |= INTR_IPTXWE;

    nor.intr = intr;
    fake_regs_set(BASE + INTR, intr);
}

/* The FIFO's head into RFDR0 and RFDR1, its fill level in 64 bit entries */
static void update_rx(void)
{
    size_t left = nor.rx_len - nor.rx_pos;
    uint8_t head[WATERMARK] = { 0 };
    uint32_t w[2];
    size_t fill = (left + 7) / 8;

    if (nor.rx_stuck)
        left = fill = 0;

    memcpy(head, nor.rx + nor.rx_pos, left < WATERMARK ? left : WATERMARK);
    memcpy(w, head, sizeof(w));
    fake_regs_set(BASE + RFDR(0), w[0]);
    fake_regs_set(BASE + RFDR(1), w[1]);
    fake_regs_set(BASE + IPRXFSTS, fill > RX_ENTRIES ? RX_ENTRIES : fill);
    update_intr();
}

static void start_read(const uint8_t *data, size_t len)
{
    memcpy(nor.rx, data, len);
    nor.rx_len = len;
    nor.rx_pos = 0;
    update_rx();
}

static void program(void)
{
    if (!nor.wel) {
        nor.ignored++;
        return;
    }

    for (size_t i = 0; i < nor.tx_len; i++) {
        uint32_t page_start = nor.addr & ~(uint32_t)(nor.page - 1);
        uint32_t a = page_start + ((nor.addr + i) & (nor.page - 1));

        nor.mem[a % CAPACITY] &= nor.tx[i];
    }

    nor.programs++;
    nor.wel = false;
    nor.busy = nor.busy_after;
}

static void erase(size_t size)
{
    if (!nor.wel) {
        nor.ignored++;
        return;
    }

    memset(nor.mem + ((nor.addr % CAPACITY) & ~(uint32_t)(size - 1)), 0xff, size);
    nor.erases++;
    nor.wel = false;
    nor.busy = nor.busy_after;
}

static void start(void)
{
    uint32_t ipcr1 = (uint32_t)fake_regs_get(BASE + IPCR1);
    uint32_t len = ipcr1 & 0xffff;
    uint8_t buf[sizeof(nor.rx)];

    nor.seq = (ipcr1 >> 16) & 0xf;
    nor.addr = (uint32_t)fake_regs_get(BASE + IPCR0);
    nor.rx_len = nor.rx_pos = 0;
    nor.writing = false;

    if ((ipcr1 >> 24) & 0x7)
        violation("ISEQNUM isn't 0");

    if (nor.seq == nor.fail_seq) {
        nor.intr |= INTR_IPCMDERR;
        update_rx();
        return;
    }

    /* A flash that is busy takes a status read only; write disable is
     * harmless, it clears itself at the end */
    if ((nor.busy > 0 || nor.stuck_busy) && nor.seq != SEQ_READ_STATUS) {
        if (nor.seq != SEQ_WRDI)
            violation("a command while the flash is busy");

        update_rx();
        return;
    }

    switch (nor.seq) {
    case SEQ_READ:
        if (nor.addr + (uint64_t)len > CAPACITY)
            violation("a read past the flash");

        for (size_t i = 0; i < len; i++)
            buf[i] = nor.mem[(nor.addr + i) % CAPACITY];

        start_read(buf, len);
        break;
    case SEQ_READ_STATUS:
        memset(buf, ((nor.busy > 0 || nor.stuck_busy) ? WIP : 0) | (nor.wel ? WEL : 0), len);

        if (nor.busy > 0)
            nor.busy--;

        start_read(buf, len);
        break;
    case SEQ_READ_ID:
        memset(buf, 0, len);
        memcpy(buf, "\x9d\x70\x17", len < 3 ? len : 3);
        start_read(buf, len);
        break;
    case SEQ_WREN:
        nor.wel = true;
        break;
    case SEQ_WRDI:
        nor.wel = false;
        break;
    case SEQ_PP:
        if (len == 0 || len > nor.page)
            violation("a page program of no bytes or more than a page");

        nor.tx_len = len;
        nor.tx_pos = 0;
        nor.writing = true;
        break;
    case SEQ_ERASE_4K:
        erase(4 * 1024);
        break;
    case SEQ_ERASE_32K:
        erase(32 * 1024);
        break;
    case SEQ_ERASE_64K:
        erase(64 * 1024);
        break;
    default:
        violation("an unknown sequence");
    }

    update_rx();
}

/* What INTR's write 1 does: pop, push, clear */
static uint64_t intr_write(uint32_t value)
{
    if ((value & INTR_IPRXWA) && nor.rx_pos < nor.rx_len) {
        nor.rx_pos += WATERMARK;

        if (nor.rx_pos > nor.rx_len)
            nor.rx_pos = nor.rx_len;
    }

    if ((value & INTR_IPTXWE) && nor.writing) {
        uint32_t w[2] = {
            (uint32_t)fake_regs_get(BASE + TFDR(0)),
            (uint32_t)fake_regs_get(BASE + TFDR(1)),
        };
        size_t n = nor.tx_len - nor.tx_pos;

        if (n > WATERMARK)
            n = WATERMARK;

        memcpy(nor.tx + nor.tx_pos, w, n);
        nor.tx_pos += n;

        if (nor.tx_pos == nor.tx_len) {
            nor.writing = false;
            program();
        }
    }

    nor.intr &= ~value;
    update_rx();
    return nor.intr;
}

static uint64_t on_write(uintptr_t addr, uint64_t value)
{
    /* The model reads no log: keep it from filling */
    fake_reg_log_len = 0;

    switch (addr - BASE) {
    case MCR0:
        return nor.swreset_stuck ? value : (value & ~(uint64_t)MCR0_SWRESET);
    case IPRXFCR:
    case IPTXFCR:
        /* The FIFOs' clear bits clear themselves */
        return value & ~(uint64_t)1;
    case FLSHCR2(0):
    case FLSHCR2(1):
    case FLSHCR2(2):
    case FLSHCR2(3):
        return value & ~(uint64_t)BIT(31);
    case IPCMD:
        if (value & 1)
            start();
        return 0;
    case INTR:
        return intr_write((uint32_t)value);
    default:
        return value;
    }
}

static const uint8_t uu[16] = "\x6f\x2b\x0a\x1e\x52\x6c\x4a\x55\x9a\x86\x7e\x08\x95\x23\xf0\x01";
static const uint32_t lut[4];

/* The EVK's flash and geometry: 4 KiB blocks, 256 byte pages */
static const struct flexspi_nor_config nor_evk = {
    .name = "nor",
    .uuid = uu,
    .port = FLEXSPI_PORT_A1,
    .capacity = CAPACITY,
    .block_size = BLOCK,
    .page_size = PAGE,
    .mfg_id = 0x9d,
    .mfg_device_type = 0x70,
    .mfg_capacity = 0x17,
    .time_page_program_ms = 100,
    .erase_cmds = {
        { .block_size = 64 * 1024, .erase_time_ms = 1000, .lut_id = SEQ_ERASE_64K },
        { .block_size = 32 * 1024, .erase_time_ms = 500, .lut_id = SEQ_ERASE_32K },
        { .block_size = 4 * 1024, .erase_time_ms = 300, .lut_id = SEQ_ERASE_4K },
    },
    .lut_id_read_status = SEQ_READ_STATUS,
    .lut_id_read_id = SEQ_READ_ID,
    .lut_id_read = SEQ_READ,
    .lut_id_write = SEQ_PP,
    .lut_id_wr_enable = SEQ_WREN,
    .lut_id_wr_disable = SEQ_WRDI,
};

/* The flash the driver is set up with: the EVK's, which a test may change */
static struct flexspi_nor_config nor_cfg;

static const struct flexspi_core_config core = {
    .base = BASE,
    .lut = lut,
    .lut_elements = 4,
    .mem_length = 1,
    .mem = { &nor_cfg },
};

static bio_dev_t dev;
static uint8_t out[128 * 1024];
static uint8_t in[128 * 1024];
static uint8_t out_device[CAPACITY];

static void reset_model(void)
{
    memset(&nor, 0, sizeof(nor));
    memset(nor.mem, 0xa5, sizeof(nor.mem));
    nor.page = PAGE;
    nor.busy_after = 2;
    nor.fail_seq = -1;

    fake_regs_reset();
    fake_regs_on_write = on_write;
    fake_regs_set(BASE + STS0, STS0_IDLE);
    update_rx();

    fake_us_tick = FAKE_US_TICK_DEFAULT;
    fake_us_tick_step = 1;
    bio_reset();
    nor_cfg = nor_evk;
}

/* The driver with nor_cfg, and the device it registers */
static int init(void)
{
    int rc = imx_flexspi_init(&core);

    if (rc == PB_OK) {
        dev = bio_get_part_by_uu(uu);
        assert_true(dev >= 0);
    }

    return rc;
}

static int setup(void **state)
{
    (void)state;
    reset_model();
    assert_int_equal(init(), PB_OK);
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    assert_int_equal(nor.violations, 0);
    return 0;
}

static void fill(uint8_t *buf, size_t len, unsigned int seed)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)(i * 7 + seed + (i >> 8));
}

static void assert_mem(size_t offset, const uint8_t *expected, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (nor.mem[offset + i] != expected[i])
            fail_msg(
                "Byte 0x%zx is 0x%02x, not 0x%02x", offset + i, nor.mem[offset + i], expected[i]);
    }
}

static void assert_mem_is(size_t offset, uint8_t value, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (nor.mem[offset + i] != value)
            fail_msg("Byte 0x%zx is 0x%02x, not 0x%02x", offset + i, nor.mem[offset + i], value);
    }
}

/* Erase, write and read back, by content, with the blocks around */
static void test_erase_write_read(void **state)
{
    (void)state;

    assert_int_equal(bio_block_size(dev), BLOCK);
    assert_int_equal(bio_size(dev), CAPACITY);

    assert_int_equal(bio_erase(dev, 16, 16), PB_OK);
    assert_mem_is(15 * BLOCK, 0xa5, BLOCK);
    assert_mem_is(16 * BLOCK, 0xff, 16 * BLOCK);
    assert_mem_is(32 * BLOCK, 0xa5, BLOCK);

    fill(out, 3 * BLOCK, 1);
    assert_int_equal(bio_write(dev, 17, 3 * BLOCK, out), PB_OK);
    assert_mem_is(16 * BLOCK, 0xff, BLOCK);
    assert_mem(17 * BLOCK, out, 3 * BLOCK);
    assert_mem_is(20 * BLOCK, 0xff, BLOCK);

    memset(in, 0, sizeof(in));
    assert_int_equal(bio_read(dev, 16, 5 * BLOCK, in), PB_OK);

    for (size_t i = 0; i < BLOCK; i++) {
        assert_int_equal(in[i], 0xff);
        assert_int_equal(in[4 * BLOCK + i], 0xff);
    }

    assert_memory_equal(in + BLOCK, out, 3 * BLOCK);
}

/* drivers.md 3: a flash that stays busy is -PB_ERR_TIMEOUT, not a hang */
static void test_flash_stays_busy(void **state)
{
    (void)state;

    assert_int_equal(bio_erase(dev, 0, 1), PB_OK);
    nor.busy_after = 1000000000;
    fill(out, BLOCK, 2);
    assert_int_equal(bio_write(dev, 0, BLOCK, out), -PB_ERR_TIMEOUT);
    assert_int_equal(nor.programs, 1);
    nor.busy = 0;
}

/* drivers.md 3: a controller that doesn't answer is -PB_ERR_TIMEOUT */
static void test_controller_stays_busy(void **state)
{
    (void)state;

    fake_regs_set(BASE + STS0, 0);
    assert_int_equal(bio_read(dev, 0, BLOCK, in), -PB_ERR_TIMEOUT);
    fake_regs_set(BASE + STS0, STS0_IDLE);
    assert_int_equal(bio_read(dev, 0, BLOCK, in), PB_OK);

    nor.rx_stuck = true;
    assert_int_equal(bio_read(dev, 0, BLOCK, in), -PB_ERR_TIMEOUT);
    nor.rx_stuck = false;

    nor.tx_stuck = true;
    update_intr();
    assert_int_equal(bio_write(dev, 0, BLOCK, out), -PB_ERR_TIMEOUT);
    nor.tx_stuck = false;
    nor.writing = false;
    nor.wel = false;
    update_intr();

    assert_int_equal(bio_read(dev, 0, BLOCK, in), PB_OK);
    assert_mem(0, in, BLOCK);
}

static void test_reset_stays(void **state)
{
    (void)state;

    reset_model();
    nor.swreset_stuck = true;
    assert_int_equal(imx_flexspi_init(&core), -PB_ERR_TIMEOUT);
}

/* F-198: an erase whose write enable failed is an error, and erases
 * nothing */
static void test_erase_write_enable_fails(void **state)
{
    (void)state;

    nor.fail_seq = SEQ_WREN;
    assert_int_equal(bio_erase(dev, 4, 1), -PB_ERR_IO);
    assert_int_equal(nor.erases, 0);
    assert_mem_is(4 * BLOCK, 0xa5, BLOCK);

    nor.fail_seq = -1;
    assert_int_equal(bio_erase(dev, 4, 1), PB_OK);
    assert_mem_is(4 * BLOCK, 0xff, BLOCK);
}

/* F-198, F-284: an erase whose busy poll failed stops there, it doesn't
 * send the next command to a busy flash */
static void test_erase_busy_poll_fails(void **state)
{
    (void)state;

    nor.fail_seq = SEQ_READ_STATUS;
    assert_int_equal(bio_erase(dev, 4, 2), -PB_ERR_IO);
    assert_int_equal(nor.erases, 1);
    nor.busy = 0;

    /* An erase that doesn't end */
    nor.fail_seq = -1;
    nor.busy_after = 1000000000;
    assert_int_equal(bio_erase(dev, 8, 2), -PB_ERR_TIMEOUT);
    assert_int_equal(nor.erases, 2);
    nor.busy = 0;
}

/* F-284: a read whose busy poll failed is that error, and reads nothing */
static void test_read_busy_poll_fails(void **state)
{
    (void)state;

    nor.fail_seq = SEQ_READ_STATUS;
    assert_int_equal(bio_read(dev, 0, BLOCK, in), -PB_ERR_IO);

    nor.fail_seq = -1;
    nor.stuck_busy = true;
    assert_int_equal(bio_read(dev, 0, BLOCK, in), -PB_ERR_TIMEOUT);
    nor.stuck_busy = false;

    assert_int_equal(bio_read(dev, 0, BLOCK, in), PB_OK);
}

/* F-198: an erase uses an erase unit only where the range covers it whole,
 * and the unit starts where the flash's does: exactly the blocks asked for,
 * none around them (bio.md 3.3) */
static void check_erase(lba_t first, size_t count)
{
    memset(nor.mem, 0xa5, sizeof(nor.mem));
    assert_int_equal(bio_erase(dev, first, count), PB_OK);
    assert_mem_is(0, 0xa5, first * BLOCK);
    assert_mem_is(first * BLOCK, 0xff, count * BLOCK);
    assert_mem_is((first + count) * BLOCK, 0xa5, CAPACITY - (first + count) * BLOCK);
}

static void test_erase_exactly_the_blocks(void **state)
{
    (void)state;

    check_erase(0, 64);
    assert_int_equal(nor.erases, 4);

    /* 64 KiB at 4 KiB */
    nor.erases = 0;
    check_erase(1, 16);
    assert_int_equal(nor.erases, 7 + 1 + 1); /* 4K * 7, 32K at block 8, 4K */

    /* 32 KiB at 16 KiB */
    check_erase(4, 8);
    check_erase(15, 18);
    check_erase(63, 1);
    check_erase(16, 16);
    check_erase(24, 17);
}

/* F-198: a range that the smallest erase unit doesn't cover whole is
 * refused, not rounded up to it */
static void test_erase_smaller_than_a_unit(void **state)
{
    (void)state;

    reset_model();
    nor_cfg.erase_cmds[2].block_size = 0;
    nor_cfg.erase_cmds[2].lut_id = 0;
    assert_int_equal(init(), PB_OK);

    assert_int_equal(bio_erase(dev, 8, 1), -PB_ERR_PARAM);
    assert_int_equal(bio_erase(dev, 4, 8), -PB_ERR_PARAM);
    assert_int_equal(nor.erases, 0);
    assert_mem_is(0, 0xa5, CAPACITY);

    check_erase(8, 8);
    check_erase(16, 24);

    /* No erase command at all */
    reset_model();
    memset(nor_cfg.erase_cmds, 0, sizeof(nor_cfg.erase_cmds));
    assert_int_equal(init(), PB_OK);
    assert_int_equal(bio_erase(dev, 0, 1), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(nor.erases, 0);
}

/* F-198: IDATSZ is 16 bits. A read of 64 KiB or more is split, not
 * truncated (bio.md 3.1: no limit on the length but the device's size) */
static void test_read_64k_and_more(void **state)
{
    static const size_t lengths[] = { 64 * 1024, 96 * 1024, 128 * 1024 };
    (void)state;

    fill(nor.mem, CAPACITY, 3);

    for (size_t i = 0; i < ARRAY_SIZE(lengths); i++) {
        lba_t first = 5 + i;

        memset(in, 0, sizeof(in));
        assert_int_equal(bio_read(dev, first, lengths[i], in), PB_OK);
        assert_memory_equal(in, nor.mem + first * BLOCK, lengths[i]);
    }

    /* The whole device */
    assert_int_equal(bio_read(dev, 0, CAPACITY, out_device), PB_OK);
    assert_memory_equal(out_device, nor.mem, CAPACITY);
}

/* A buffer of len bytes that ends where a page nobody may touch starts:
 * a read past it faults */
static uint8_t *guarded(size_t len, uint8_t **map, size_t *map_len)
{
    size_t page = (size_t)sysconf(_SC_PAGESIZE);

    assert_true(len <= page);
    *map_len = 2 * page;
    *map = mmap(NULL, *map_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert_true(*map != MAP_FAILED);
    assert_int_equal(mprotect(*map + page, page, PROT_NONE), 0);
    return *map + page - len;
}

/* F-284: a write reads exactly the length of the buffer (bio.md 3.2), also
 * when the last piece for the FIFO isn't whole words. A flash with 4 byte
 * pages makes every page program such a piece */
static void test_write_reads_only_the_buffer(void **state)
{
    static const size_t pages[] = { 4, 8, 16, PAGE };
    uint8_t *map;
    size_t map_len;
    uint8_t *buf = guarded(BLOCK, &map, &map_len);
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(pages); i++) {
        reset_model();
        nor_cfg.page_size = pages[i];
        nor.page = pages[i];
        assert_int_equal(init(), PB_OK);

        fill(buf, BLOCK, (unsigned int)i);
        assert_int_equal(bio_erase(dev, 3, 1), PB_OK);
        assert_int_equal(bio_write(dev, 3, BLOCK, buf), PB_OK);
        assert_mem_is(2 * BLOCK, 0xa5, BLOCK);
        assert_mem(3 * BLOCK, buf, BLOCK);
        assert_mem_is(4 * BLOCK, 0xa5, BLOCK);
        assert_int_equal(nor.violations, 0);
    }

    munmap(map, map_len);
}

/* F-284: the caller's buffer is bytes, whatever its alignment. The host
 * takes unaligned words, so this fails under UBSan (the sanitizer build)
 * only, as it would fault on a core that doesn't */
static void test_unaligned_buffer(void **state)
{
    (void)state;

    for (size_t off = 1; off < 4; off++) {
        uint8_t *buf = out + off;

        assert_int_equal(bio_erase(dev, 2, 2), PB_OK);
        fill(buf, 2 * BLOCK, (unsigned int)off);
        assert_int_equal(bio_write(dev, 2, 2 * BLOCK, buf), PB_OK);
        assert_mem(2 * BLOCK, buf, 2 * BLOCK);

        memset(in, 0, sizeof(in));
        assert_int_equal(bio_read(dev, 2, 2 * BLOCK, in + off), PB_OK);
        assert_memory_equal(in + off, buf, 2 * BLOCK);
    }
}

/* F-284: a read of a length that isn't a multiple of 4 reads it, exactly
 * (bio.md 3.1), where it was refused after the command had started. A
 * flash with 1 byte blocks gives the driver such lengths */
static void test_read_any_length(void **state)
{
    static const struct {
        lba_t first;
        size_t len;
    } reads[] = { { 5, 1 }, { 9, 3 }, { 17, 6 }, { 100, 13 }, { 4093, 7 }, { 0, 4 } };

    uint8_t *map;
    size_t map_len;
    uint8_t *page_end = guarded(0, &map, &map_len);
    (void)state;

    reset_model();
    nor_cfg.block_size = 1;
    assert_int_equal(init(), PB_OK);
    fill(nor.mem, CAPACITY, 5);

    for (size_t i = 0; i < ARRAY_SIZE(reads); i++) {
        uint8_t *buf = page_end - reads[i].len;

        assert_int_equal(bio_read(dev, reads[i].first, reads[i].len, buf), PB_OK);
        assert_memory_equal(buf, nor.mem + reads[i].first, reads[i].len);
    }

    munmap(map, map_len);
}

/* The codes of drivers.md 3: a port the controller doesn't have is the
 * caller's argument */
static void test_port_out_of_range(void **state)
{
    (void)state;

    reset_model();
    nor_cfg.port = FLEXSPI_PORT_COUNT;
    assert_int_equal(imx_flexspi_init(&core), -PB_ERR_PARAM);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_erase_write_read, setup, teardown),
        cmocka_unit_test_setup_teardown(test_flash_stays_busy, setup, teardown),
        cmocka_unit_test_setup_teardown(test_controller_stays_busy, setup, teardown),
        cmocka_unit_test_setup_teardown(test_reset_stays, setup, teardown),
        cmocka_unit_test_setup_teardown(test_erase_write_enable_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_erase_busy_poll_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_busy_poll_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_erase_exactly_the_blocks, setup, teardown),
        cmocka_unit_test_setup_teardown(test_erase_smaller_than_a_unit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_64k_and_more, setup, teardown),
        cmocka_unit_test_setup_teardown(test_write_reads_only_the_buffer, setup, teardown),
        cmocka_unit_test_setup_teardown(test_unaligned_buffer, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_any_length, setup, teardown),
        cmocka_unit_test_setup_teardown(test_port_out_of_range, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
