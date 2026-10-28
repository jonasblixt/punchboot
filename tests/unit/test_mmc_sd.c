/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * mmc_core with SD cards (models/sd_model.h), through the fake HAL that
 * behaves like a correct controller (models/hal_model.h).
 *
 * Every test fails on a violation of the SD protocol, see models/bench.h.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <string.h>

#include "models/bench.h"
#include "models/hal_model.h"
#include "models/sd_model.h"

#define CHUNK_BYTES (16 * 1024)

#define UU_SD \
    (const unsigned char *)"\x3b\x1a\x5e\x22\x8d\x10\x4c\x2a\x9e\x41\x6f\x80\x0d\x52\xc7\x19"

static struct sd_model_config card_cfg;
static struct mmc_device_config dev_cfg;
static uint8_t out[64 * 1024];
static uint8_t in[sizeof(out)];
static uint8_t check[sizeof(out)];

static int setup(void **state)
{
    (void)state;

    bench_reset();
    bio_reset();
    sd_model_default_config(&card_cfg);

    dev_cfg = (struct mmc_device_config){
        .card = MMC_CARD_TYPE_SD,
        .mode = MMC_BUS_MODE_SD_HS,
        .width = MMC_BUS_WIDTH_4BIT,
        .user_uu = UU_SD,
    };
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    sd_model_free();
    return 0;
}

static int init(void)
{
    struct bus_card *card = sd_model_init(&card_cfg);

    return mmc_init(hal_model_init(card, CHUNK_BYTES), &dev_cfg, NULL);
}

static bio_dev_t sd(void)
{
    bio_dev_t d = bio_get_part_by_uu(UU_SD);

    assert_true(d >= 0);
    return d;
}

static void fill(uint8_t *buf, size_t len, unsigned int seed)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)(i * 11 + seed + (i >> 9));
}

static void read_write(uint64_t lba)
{
    fill(out, sizeof(out), (unsigned int)lba);

    assert_int_equal(bio_write(sd(), lba, sizeof(out), out), PB_OK);
    sd_model_read(lba, check, sizeof(check) / 512);
    assert_memory_equal(out, check, sizeof(out));
    assert_int_equal(bio_read(sd(), lba, sizeof(in), in), PB_OK);
    assert_memory_equal(out, in, sizeof(in));

    assert_int_equal(bio_write(sd(), lba + 300, 512, out + 1024), PB_OK);
    assert_int_equal(bio_read(sd(), lba + 300, 512, in), PB_OK);
    assert_memory_equal(out + 1024, in, 512);
}

static void test_sdhc_high_speed(void **state)
{
    (void)state;

    assert_int_equal(init(), PB_OK);
    assert_int_equal(sd_model_speed(), SD_HIGH_SPEED);
    assert_int_equal(sd_model_bus_width(), 4);
    assert_int_equal(hal_model_bus()->clk_hz, 50000000);
    assert_int_equal(hal_model_bus()->width, 4);
    assert_int_equal(bio_get_last_block(sd()), card_cfg.sectors - 1);
    read_write(4096);
    read_write(card_cfg.sectors - 512);
    assert_bench_clean();
}

static void test_sdxc(void **state)
{
    (void)state;

    card_cfg.sectors = 128ULL * 1024 * 1024 * 2; /* 128 GiB */
    assert_int_equal(init(), PB_OK);
    assert_int_equal(bio_get_last_block(sd()), card_cfg.sectors - 1);
    read_write(100000000);
    assert_bench_clean();
}

/* SDSC: byte addresses, CMD16 */
static void test_sdsc(void **state)
{
    (void)state;

    card_cfg.sectors = 1024 * 1024 * 2; /* 1 GiB */
    assert_int_equal(init(), PB_OK);
    assert_int_equal(bio_get_last_block(sd()), card_cfg.sectors - 1);
    read_write(1000);
    assert_bench_clean();
}

/* Physical layer 1.x: no answer to CMD8 */
static void test_sdsc_v1(void **state)
{
    (void)state;

    card_cfg.sectors = 256 * 1024 * 2; /* 256 MiB */
    card_cfg.v1 = true;
    assert_int_equal(init(), PB_OK);
    assert_int_equal(sd_model_speed(), SD_DEFAULT_SPEED); /* SD_SPEC 1.0 has no CMD6 */
    assert_true(hal_model_bus()->clk_hz <= 25000000);
    read_write(77);
    assert_bench_clean();
}

/* The board allows default speed on one data line */
static void test_default_speed_1bit(void **state)
{
    (void)state;

    dev_cfg.mode = MMC_BUS_MODE_SD_DEFAULT;
    dev_cfg.width = MMC_BUS_WIDTH_1BIT;
    assert_int_equal(init(), PB_OK);
    assert_int_equal(sd_model_speed(), SD_DEFAULT_SPEED);
    assert_int_equal(sd_model_bus_width(), 1);
    assert_int_equal(hal_model_bus()->clk_hz, 25000000);
    read_write(0);
    assert_bench_clean();
}

static void test_slow_power_up(void **state)
{
    (void)state;

    card_cfg.op_cond_busy = 60; /* 600 ms, the limit is 1 s */
    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
}

static void test_no_card(void **state)
{
    (void)state;

    struct bus_card *card = sd_model_init(&card_cfg);

    sd_model_insert(false);
    assert_int_not_equal(mmc_init(hal_model_init(card, CHUNK_BYTES), &dev_cfg, NULL), PB_OK);
    assert_bench_clean();
}

static void test_data_errors(void **state)
{
    (void)state;

    assert_int_equal(init(), PB_OK);
    sd_model_fail_read(BUS_DATA_CRC);
    assert_int_not_equal(bio_read(sd(), 0, 4096, in), PB_OK);
    assert_int_equal(bio_read(sd(), 0, 4096, in), PB_OK);
    sd_model_fail_write(BUS_DATA_CRC);
    assert_int_not_equal(bio_write(sd(), 0, 4096, out), PB_OK);
    assert_int_equal(bio_write(sd(), 0, 4096, out), PB_OK);
    assert_bench_clean();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_sdhc_high_speed, setup, teardown),
        cmocka_unit_test_setup_teardown(test_sdxc, setup, teardown),
        cmocka_unit_test_setup_teardown(test_sdsc, setup, teardown),
        cmocka_unit_test_setup_teardown(test_sdsc_v1, setup, teardown),
        cmocka_unit_test_setup_teardown(test_default_speed_1bit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_slow_power_up, setup, teardown),
        cmocka_unit_test_setup_teardown(test_no_card, setup, teardown),
        cmocka_unit_test_setup_teardown(test_data_errors, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
