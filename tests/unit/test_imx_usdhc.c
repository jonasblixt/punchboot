/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The uSDHC driver and mmc_core together, against a register model of the
 * uSDHC (models/usdhc_model.h) with the e.MMC model on its bus. The boards'
 * configurations are copied from their board.c.
 *
 * Every test fails on a violation of the uSDHC RM or JESD84-B51.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <arch/arch.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <string.h>

/* A test of an open bug, it runs once the finding is fixed (testing.md) */
#define SKIP_OPEN(finding)                             \
    do {                                               \
        print_message("Skipped, open: %s\n", finding); \
        skip();                                        \
    } while (0)

#include "models/bench.h"
#include "models/emmc_model.h"
#include "models/usdhc_model.h"

#define IMX6UL_USDHC1_BASE 0x02190000
#define IMX8M_USDHC3_BASE  0x30b60000

#define UU_BOOT0 \
    (const unsigned char *)"\x9e\xef\x75\x44\xbf\x68\x4b\xf7\x86\x78\xda\x11\x7c\xbc\xcb\xa8"
#define UU_BOOT1 \
    (const unsigned char *)"\x4e\xe3\x16\x90\x0c\x9b\x4d\x56\xa6\xa6\xe6\xd6\xec\xfd\x4d\x54"
#define UU_USER \
    (const unsigned char *)"\x1a\xad\x85\xa9\x75\xcd\x42\x6d\x8d\xc4\xe9\xbd\xfe\xeb\x68\x75"
#define UU_RPMB \
    (const unsigned char *)"\x8d\x75\xd8\xb9\xb1\x69\x4d\xe6\xbe\xe0\x48\xab\xdc\x95\xc4\x08"

/* src/board/imx8mpevk/board.c */
static const struct imx_usdhc_config imx8mpevk_emmc = {
    .base = IMX8M_USDHC3_BASE,
    .delay_tap = 35,
    .mmc_config = {
        .mode = MMC_BUS_MODE_HS200,
        .width = MMC_BUS_WIDTH_8BIT,
        .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
        .boot0_uu = UU_BOOT0,
        .boot1_uu = UU_BOOT1,
        .user_uu = UU_USER,
        .rpmb_uu = UU_RPMB,
    },
};

/* src/board/jiffy/board.c */
static const struct imx_usdhc_config jiffy_emmc = {
    .base = IMX6UL_USDHC1_BASE,
    .delay_tap = 0,
    .mmc_config = {
        .mode = MMC_BUS_MODE_DDR52,
        .width = MMC_BUS_WIDTH_8BIT_DDR,
        .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
        .boot0_uu = UU_BOOT0,
        .boot1_uu = UU_BOOT1,
        .user_uu = UU_USER,
        .rpmb_uu = UU_RPMB,
    },
};

static struct emmc_model_config card_cfg;

/* DMA buffers must be below 4 GiB (usdhc_model.h) */
static uint8_t out[256 * 1024];
static uint8_t in[sizeof(out)];
static uint8_t check[sizeof(out)];

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    if (!usdhc_model_owns(addr) || width != 32)
        fail_msg("Unexpected %u bit read at 0x%lx", width, (unsigned long)addr);
    return usdhc_model_read(addr);
}

void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width)
{
    if (!usdhc_model_owns(addr) || width != 32)
        fail_msg("Unexpected %u bit write at 0x%lx", width, (unsigned long)addr);
    usdhc_model_write(addr, (uint32_t)value);
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

static int setup(void **state)
{
    (void)state;

    bench_reset();
    bio_reset();
    emmc_model_default_config(&card_cfg);
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    emmc_model_free();
    return 0;
}

static int init(const struct imx_usdhc_config *cfg, enum usdhc_soc soc, unsigned int clk_hz)
{
    struct usdhc_model_config m = {
        .base = cfg->base,
        .base_clk_hz = clk_hz,
        .soc = soc,
        .card = emmc_model_init(&card_cfg),
        .vccq_mv = card_cfg.vccq_mv,
    };

    usdhc_model_init(&m);
    return imx_usdhc_init(cfg, clk_hz);
}

static bio_dev_t part(const unsigned char *uu)
{
    bio_dev_t d = bio_get_part_by_uu(uu);

    assert_true(d >= 0);
    return d;
}

static void fill(uint8_t *buf, size_t len, unsigned int seed)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)(i * 13 + seed + (i >> 9));
}

static void read_write(void)
{
    fill(out, sizeof(out), 3);

    /* Multiple blocks, more than one ADMA2 descriptor */
    assert_int_equal(bio_write(part(UU_USER), 2048, sizeof(out), out), PB_OK);
    emmc_model_read(EMMC_PART_USER, 2048, check, sizeof(check) / 512);
    assert_memory_equal(out, check, sizeof(out));
    assert_int_equal(bio_read(part(UU_USER), 2048, sizeof(in), in), PB_OK);
    assert_memory_equal(out, in, sizeof(in));

    /* Single blocks */
    assert_int_equal(bio_write(part(UU_USER), 5, 512, out + 4096), PB_OK);
    assert_int_equal(bio_read(part(UU_USER), 5, 512, in), PB_OK);
    assert_memory_equal(out + 4096, in, 512);

    /* A boot partition */
    assert_int_equal(bio_write(part(UU_BOOT1), 0, 64 * 1024, out), PB_OK);
    assert_int_equal(bio_write_done(part(UU_BOOT1)), PB_OK);
    assert_int_equal(bio_read(part(UU_BOOT1), 0, 64 * 1024, in), PB_OK);
    assert_memory_equal(out, in, 64 * 1024);
}

static void test_imx8mpevk_emmc(void **state)
{
    SKIP_OPEN("F-122, F-125");
    (void)state;

    assert_int_equal(init(&imx8mpevk_emmc, USDHC_IMX8M, 200000000), PB_OK);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_HS_TIMING], 2);
    assert_int_equal(usdhc_model_bus()->clk_hz, 200000000);
    assert_int_equal(usdhc_model_bus()->width, 8);
    read_write();
    assert_bench_clean();
}

static void test_jiffy_emmc(void **state)
{
    SKIP_OPEN("F-122, F-123, F-125");
    (void)state;

    card_cfg.vccq_mv = 3300;
    assert_int_equal(init(&jiffy_emmc, USDHC_IMX6UL, 198000000), PB_OK);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_BUS_WIDTH], 6);
    read_write();
    assert_bench_clean();
}

/* RPMB: CMD23 + CMD18, no CMD12 after a counted transfer (6.6.7.1) */
static void test_rpmb_read(void **state)
{
    SKIP_OPEN("F-124");
    (void)state;

    assert_int_equal(init(&imx8mpevk_emmc, USDHC_IMX8M, 200000000), PB_OK);
    assert_int_equal(bio_read(part(UU_RPMB), 0, 512, in), PB_OK);
    assert_bench_clean();
}

static void test_read_crc_error(void **state)
{
    SKIP_OPEN("F-124");
    (void)state;

    assert_int_equal(init(&imx8mpevk_emmc, USDHC_IMX8M, 200000000), PB_OK);
    emmc_model_fail_read(BUS_DATA_CRC);
    assert_int_not_equal(bio_read(part(UU_USER), 0, 4096, in), PB_OK);
    assert_int_equal(bio_read(part(UU_USER), 0, 4096, in), PB_OK);
    assert_bench_clean();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_imx8mpevk_emmc, setup, teardown),
        cmocka_unit_test_setup_teardown(test_jiffy_emmc, setup, teardown),
        cmocka_unit_test_setup_teardown(test_rpmb_read, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_crc_error, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
