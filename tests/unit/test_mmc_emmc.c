/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * mmc_core against the e.MMC model (models/emmc_model.h), through a fake
 * HAL that behaves like a correct controller (models/hal_model.h).
 *
 * Every test fails on a JESD84-B51 violation, see models/bench.h.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

/* A test of an open bug, it runs once the finding is fixed (testing.md) */
#define SKIP_OPEN(finding)                             \
    do {                                               \
        print_message("Skipped, open: %s\n", finding); \
        skip();                                        \
    } while (0)

#include "models/bench.h"
#include "models/emmc_model.h"
#include "models/hal_model.h"

#define CHUNK_BYTES (16 * 1024)

#define UU_BOOT0 \
    (const unsigned char *)"\x9e\xef\x75\x44\xbf\x68\x4b\xf7\x86\x78\xda\x11\x7c\xbc\xcb\xa8"
#define UU_BOOT1 \
    (const unsigned char *)"\x4e\xe3\x16\x90\x0c\x9b\x4d\x56\xa6\xa6\xe6\xd6\xec\xfd\x4d\x54"
#define UU_USER \
    (const unsigned char *)"\x1a\xad\x85\xa9\x75\xcd\x42\x6d\x8d\xc4\xe9\xbd\xfe\xeb\x68\x75"
#define UU_RPMB \
    (const unsigned char *)"\x8d\x75\xd8\xb9\xb1\x69\x4d\xe6\xbe\xe0\x48\xab\xdc\x95\xc4\x08"

/* Table 68 */
#define ST_ERROR BIT(19)

static struct emmc_model_config card_cfg;
static struct mmc_device_config dev_cfg;
static struct bus_card *card;
static struct mmc_device *mmc;

static int setup(void **state)
{
    (void)state;

    bench_reset();
    bio_reset();
    emmc_model_default_config(&card_cfg);
    hal_model_set_window(0, 127);
    hal_model_set_gap(1, 0);

    dev_cfg = (struct mmc_device_config){
        .mode = MMC_BUS_MODE_HS200,
        .width = MMC_BUS_WIDTH_8BIT,
        .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
        .boot0_uu = UU_BOOT0,
        .boot1_uu = UU_BOOT1,
        .user_uu = UU_USER,
        .rpmb_uu = UU_RPMB,
    };
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    emmc_model_free();
    return 0;
}

static int init(void)
{
    card = emmc_model_init(&card_cfg);
    return mmc_init(hal_model_init(card, CHUNK_BYTES), &dev_cfg, &mmc);
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
        buf[i] = (uint8_t)(i * 7 + seed + (i >> 9));
}

static void test_hs200(void **state)
{
    (void)state;

    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_HS_TIMING], 2);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_BUS_WIDTH], 2);
    assert_int_equal(hal_model_bus()->clk_hz, 200000000);
    assert_int_equal(emmc_model_state(), EMMC_TRAN);
}

static void test_ddr52(void **state)
{
    (void)state;

    dev_cfg.mode = MMC_BUS_MODE_DDR52;
    dev_cfg.width = MMC_BUS_WIDTH_8BIT_DDR;

    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_HS_TIMING], 1);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_BUS_WIDTH], 6);
    assert_true(hal_model_bus()->ddr);
}

static void test_ddr52_4bit(void **state)
{
    (void)state;

    dev_cfg.mode = MMC_BUS_MODE_DDR52;
    dev_cfg.width = MMC_BUS_WIDTH_4BIT_DDR;

    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_BUS_WIDTH], 5);
}

/* HS200 tuning finds the middle of the taps that work, quickly */
static void test_tuning(void **state)
{
    (void)state;

    hal_model_set_window(16, 77);
    uint64_t t0 = bench_now_us();

    assert_int_equal(init(), PB_OK);
    assert_int_equal(hal_model_tap(), 46);
    assert_int_equal(emmc_model_cmd_count(MMC_CMD_SEND_TUNING_BLOCK_HS200), 128);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_HS_TIMING], 2);
    print_message("Init with tuning: %llu us simulated\n",
                  (unsigned long long)(bench_now_us() - t0));
    assert_bench_clean();
}

/* A narrow window, not on a coarse step */
static void test_tuning_narrow(void **state)
{
    (void)state;

    hal_model_set_window(41, 43);
    assert_int_equal(init(), PB_OK);
    assert_int_equal(hal_model_tap(), 42);
    assert_bench_clean();
}

/* Narrow failing gaps, as on the imx8mpevk (taps 14-15 and 78-79) */
static void test_tuning_gap(void **state)
{
    (void)state;

    hal_model_set_window(16, 127);
    hal_model_set_gap(78, 79);
    assert_int_equal(init(), PB_OK);
    assert_int_equal(hal_model_tap(), 46);
    assert_bench_clean();
}

/* A window up to the last tap */
static void test_tuning_window_at_end(void **state)
{
    (void)state;

    hal_model_set_window(90, 127);
    assert_int_equal(init(), PB_OK);
    assert_int_equal(hal_model_tap(), 108);
    assert_bench_clean();
}

/* No tap works: high speed, the boot goes on */
static void test_tuning_fails(void **state)
{
    static uint8_t buf[4096];
    (void)state;

    hal_model_set_window(1, 0);
    assert_int_equal(init(), PB_OK);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_HS_TIMING], 1);
    assert_int_equal(hal_model_bus()->clk_hz, 52000000);
    assert_int_equal(bio_read(part(UU_USER), 0, sizeof(buf), buf), PB_OK);
    assert_bench_clean();
}

/* HS400 with Enhanced Strobe needs no tuning, 6.6.2.3 */
static void test_hs400_enhanced_strobe(void **state)
{
    (void)state;

    dev_cfg.mode = MMC_BUS_MODE_HS400;
    dev_cfg.width = MMC_BUS_WIDTH_8BIT_DDR_STROBE;

    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_HS_TIMING], 3);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_BUS_WIDTH], 0x86);
}

/* HS400 without Enhanced Strobe: tuned in HS200 first, 6.6.2.3 */
static void test_hs400(void **state)
{
    SKIP_OPEN("F-116");
    (void)state;

    dev_cfg.mode = MMC_BUS_MODE_HS400;
    dev_cfg.width = MMC_BUS_WIDTH_8BIT_DDR;

    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_HS_TIMING], 3);
}

/* The device has no HS200: the host must check DEVICE_TYPE, 6.6.2.2 */
static void test_hs200_not_supported(void **state)
{
    (void)state;

    card_cfg.device_type = BIT(2) | BIT(1) | BIT(0);

    assert_int_equal(init(), -PB_ERR_NOT_SUPPORTED);
    assert_bench_clean();
}

static void test_read_write(void **state)
{
    static uint8_t out[64 * 1024];
    static uint8_t in[sizeof(out)];
    static uint8_t check[sizeof(out)];
    (void)state;

    assert_int_equal(init(), PB_OK);

    fill(out, sizeof(out), 1);
    assert_int_equal(bio_write(part(UU_USER), 1000, sizeof(out), out), PB_OK);
    emmc_model_read(EMMC_PART_USER, 1000, check, sizeof(check) / 512);
    assert_memory_equal(out, check, sizeof(out));

    assert_int_equal(bio_read(part(UU_USER), 1000, sizeof(in), in), PB_OK);
    assert_memory_equal(out, in, sizeof(in));

    /* Single blocks */
    assert_int_equal(bio_write(part(UU_USER), 7, 512, out + 512), PB_OK);
    assert_int_equal(bio_read(part(UU_USER), 7, 512, in), PB_OK);
    assert_memory_equal(out + 512, in, 512);

    /* The last block of the user area */
    uint64_t last = card_cfg.user_sectors - 1;
    assert_int_equal(bio_write(part(UU_USER), last, 512, out), PB_OK);
    assert_int_equal(bio_read(part(UU_USER), last, 512, in), PB_OK);
    assert_memory_equal(out, in, 512);

    assert_bench_clean();
}

/* Up to 2 GiB: byte addresses, JESD84-B51 6.4.2 */
static void test_byte_addressing(void **state)
{
    static uint8_t out[16 * 1024];
    static uint8_t check[sizeof(out)];
    (void)state;

    card_cfg.user_sectors = 1024 * 1024 * 1024 / 512;
    assert_int_equal(init(), PB_OK);

    fill(out, sizeof(out), 4);
    assert_int_equal(bio_write(part(UU_USER), 1000, sizeof(out), out), PB_OK);
    emmc_model_read(EMMC_PART_USER, 1000, check, sizeof(check) / 512);
    assert_memory_equal(out, check, sizeof(out));
    assert_int_equal(bio_read(part(UU_USER), 3, 512, check), PB_OK);
    assert_bench_clean();
}

static void test_boot_partitions(void **state)
{
    static uint8_t out[8 * 1024];
    static uint8_t check[sizeof(out)];
    (void)state;

    assert_int_equal(init(), PB_OK);

    fill(out, sizeof(out), 2);
    assert_int_equal(bio_write(part(UU_BOOT1), 0, sizeof(out), out), PB_OK);
    assert_int_equal(bio_write_done(part(UU_BOOT1)), PB_OK);
    emmc_model_read(EMMC_PART_BOOT2, 0, check, sizeof(check) / 512);
    assert_memory_equal(out, check, sizeof(out));

    /* BOOT_PARTITION_ENABLE: boot partition 2 */
    assert_int_equal((emmc_model_ext_csd()[EXT_CSD_PART_CONFIG] >> 3) & 7, 2);

    assert_int_equal(bio_read(part(UU_USER), 0, 512, out), PB_OK);
    assert_int_equal(emmc_model_ext_csd()[EXT_CSD_PART_CONFIG] & 7, 0);
    assert_bench_clean();
}

/* RPMB reads are CMD23 + CMD18 of whole frames, 6.6.22 */
static void test_rpmb_read(void **state)
{
    uint8_t frame[512];
    (void)state;

    assert_int_equal(init(), PB_OK);
    assert_int_equal(bio_read(part(UU_RPMB), 0, sizeof(frame), frame), PB_OK);
    /* No request was sent: the result is "key not programmed" */
    assert_int_equal(frame[509], 0x07);
    assert_bench_clean();
}

/* A boot ROM may leave the device selected in DDR mode */
static void test_rom_handover(void **state)
{
    (void)state;

    card = emmc_model_init(&card_cfg);
    emmc_model_rom_handover(1, 6);
    assert_int_equal(mmc_init(hal_model_init(card, CHUNK_BYTES), &dev_cfg, NULL), PB_OK);
    assert_bench_clean();
}

/* A device that takes half a second to power up (the limit is 1 s) */
static void test_slow_power_up(void **state)
{
    (void)state;

    card_cfg.op_cond_busy = 50;
    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
}

/* Out of identification mode the EXT_CSD is read at TRAN_SPEED, not 400 kHz (6.6) */
static void test_ext_csd_at_tran_speed(void **state)
{
    (void)state;

    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
    assert_int_equal(emmc_model_cmd_first_clk_hz(8), 26000000);
}

/* A device with a 20 MHz TRAN_SPEED is not clocked faster before the bus mode switch */
static void test_tran_speed_20mhz(void **state)
{
    (void)state;

    card_cfg.tran_speed = TRAN_SPEED_20MHZ;
    assert_int_equal(init(), PB_OK);
    assert_bench_clean();
    assert_int_equal(emmc_model_cmd_first_clk_hz(8), 20000000);
}

/* GENERIC_CMD6_TIME and PARTITION_SWITCH_TIME 0: not defined (7.4.31, 7.4.56) */
static void test_cmd6_times_not_defined(void **state)
{
    uint8_t buf[512];
    (void)state;

    card_cfg.generic_cmd6_time = 0;
    card_cfg.partition_switch_time = 0;

    assert_int_equal(init(), PB_OK);
    assert_int_equal(bio_read(part(UU_BOOT0), 0, sizeof(buf), buf), PB_OK);
    assert_bench_clean();
}

/* A failed partition switch must not send the access to another partition */
static void test_part_switch_error(void **state)
{
    uint8_t buf[512];
    (void)state;

    assert_int_equal(init(), PB_OK);
    unsigned int blocks = emmc_model_blocks_read();

    emmc_model_fail_switch(EXT_CSD_PART_CONFIG);
    assert_int_not_equal(bio_read(part(UU_BOOT0), 0, sizeof(buf), buf), PB_OK);
    assert_int_equal(emmc_model_blocks_read(), blocks);
}

/* An execution error during a read (Table 68, ERROR) must fail the read */
static void test_read_status_error(void **state)
{
    uint8_t buf[2048];
    (void)state;

    assert_int_equal(init(), PB_OK);
    emmc_model_status_error(MMC_CMD_READ_MULTIPLE_BLOCK, ST_ERROR);
    assert_int_not_equal(bio_read(part(UU_USER), 0, sizeof(buf), buf), PB_OK);
}

static void test_read_crc_error(void **state)
{
    uint8_t buf[2048];
    (void)state;

    assert_int_equal(init(), PB_OK);
    emmc_model_fail_read(BUS_DATA_CRC);
    assert_int_not_equal(bio_read(part(UU_USER), 0, sizeof(buf), buf), PB_OK);
    /* The next read works */
    assert_int_equal(bio_read(part(UU_USER), 0, sizeof(buf), buf), PB_OK);
    assert_bench_clean();
}

static void test_write_crc_error(void **state)
{
    uint8_t buf[2048] = { 0 };
    (void)state;

    assert_int_equal(init(), PB_OK);
    emmc_model_fail_write(BUS_DATA_CRC);
    assert_int_not_equal(bio_write(part(UU_USER), 0, sizeof(buf), buf), PB_OK);
    assert_int_equal(bio_write(part(UU_USER), 0, sizeof(buf), buf), PB_OK);
    assert_bench_clean();
}

static void test_cmd_crc_error(void **state)
{
    uint8_t buf[512];
    (void)state;

    assert_int_equal(init(), PB_OK);
    emmc_model_fail_cmd(MMC_CMD_READ_SINGLE_BLOCK, EMMC_FAULT_RSP_CRC);
    assert_int_not_equal(bio_read(part(UU_USER), 0, sizeof(buf), buf), PB_OK);
}

/* Power off notification, then power off, 6.6.36 */
static void test_power_off(void **state)
{
    (void)state;

    assert_int_equal(init(), PB_OK);
    assert_int_equal(mmc_power_off(mmc), PB_OK);
    emmc_model_power_cycle();
    assert_bench_clean();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_hs200, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ddr52, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ddr52_4bit, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tuning, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tuning_narrow, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tuning_gap, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tuning_window_at_end, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tuning_fails, setup, teardown),
        cmocka_unit_test_setup_teardown(test_hs400_enhanced_strobe, setup, teardown),
        cmocka_unit_test_setup_teardown(test_hs400, setup, teardown),
        cmocka_unit_test_setup_teardown(test_hs200_not_supported, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_write, setup, teardown),
        cmocka_unit_test_setup_teardown(test_byte_addressing, setup, teardown),
        cmocka_unit_test_setup_teardown(test_boot_partitions, setup, teardown),
        cmocka_unit_test_setup_teardown(test_rpmb_read, setup, teardown),
        cmocka_unit_test_setup_teardown(test_rom_handover, setup, teardown),
        cmocka_unit_test_setup_teardown(test_slow_power_up, setup, teardown),
        cmocka_unit_test_setup_teardown(test_part_switch_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_status_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_read_crc_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_write_crc_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cmd_crc_error, setup, teardown),
        cmocka_unit_test_setup_teardown(test_power_off, setup, teardown),
        cmocka_unit_test_setup_teardown(test_cmd6_times_not_defined, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ext_csd_at_tran_speed, setup, teardown),
        cmocka_unit_test_setup_teardown(test_tran_speed_20mhz, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
