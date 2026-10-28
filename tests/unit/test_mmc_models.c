/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Self test of the card models: a scripted host runs the sequences the
 * standards describe and must get through without a violation, and known
 * mistakes must be reported. This checks the models, not pb's drivers.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <string.h>

#include "models/bench.h"
#include "models/emmc_model.h"
#include "models/sd_model.h"

#define R1_STATE(r)                (((r) >> 9) & 0xf)
#define SWITCH_WRITE(index, value) ((3u << 24) | ((index) << 16) | ((value) << 8))

static struct bus_card *card;
static struct bus_cfg bus;
static uint8_t buf[64 * 1024];

static int setup(void **state)
{
    (void)state;
    bench_reset();
    memset(&bus, 0, sizeof(bus));
    bus.width = 1;
    bus.vdd_mv = 3300;
    bus.vccq_mv = 3300;
    return 0;
}

static int teardown(void **state)
{
    (void)state;
    emmc_model_free();
    sd_model_free();
    return 0;
}

static void set_bus(unsigned int clk_hz, unsigned int width, bool ddr)
{
    bus.clk_hz = clk_hz;
    bus.width = width;
    bus.ddr = ddr;
    card->ops->set_bus(card, &bus);
}

static void wait_busy(void)
{
    while (card->ops->busy(card)) {
        bench_advance_us(10);
        bench_check_stuck("busy");
    }
}

static struct bus_rsp cmd(uint8_t idx, uint32_t arg, enum bus_rsp_type type)
{
    struct bus_cmd c = { .idx = idx, .arg = arg, .rsp = type };
    struct bus_rsp r;

    card->ops->cmd(card, &c, &r);
    if (type == BUS_RSP_48_BUSY && !r.timeout)
        wait_busy();
    return r;
}

static uint32_t r1(uint8_t idx, uint32_t arg)
{
    struct bus_rsp r = cmd(idx, arg, BUS_RSP_48);

    assert_false(r.timeout);
    assert_false(r.crc_error);
    assert_int_equal(r.idx, idx);
    return r.r[0];
}

static uint32_t r1b(uint8_t idx, uint32_t arg)
{
    struct bus_rsp r = cmd(idx, arg, BUS_RSP_48_BUSY);

    assert_false(r.timeout);
    return r.r[0];
}

static void read_blocks(size_t len, size_t n)
{
    for (size_t i = 0; i < n; i++)
        assert_int_equal(card->ops->read(card, buf + i * len, len), BUS_DATA_OK);
}

static void write_blocks(size_t n)
{
    for (size_t i = 0; i < n; i++) {
        wait_busy();
        assert_int_equal(card->ops->write(card, buf + i * 512, 512), BUS_DATA_OK);
    }
    wait_busy();
}

/* e.MMC ------------------------------------------------------------------ */

#define EMMC_RCA 2

/* 6.4: CMD0, CMD1 until ready, CMD2, CMD3, CMD9, CMD7 */
static void emmc_identify(void)
{
    struct emmc_model_config cfg;
    struct bus_rsp r;

    emmc_model_default_config(&cfg);
    card = emmc_model_init(&cfg);
    bus.vccq_mv = 1800;
    set_bus(400000, 1, false);

    cmd(0, 0, BUS_RSP_NONE);
    do {
        r = cmd(1, 0x40ff8080, BUS_RSP_48);
        assert_false(r.timeout);
        bench_advance_us(1000);
    } while (!(r.r[0] & (1u << 31)));
    assert_int_equal(r.r[0] & (3u << 29), 2u << 29); /* Sector mode */

    r = cmd(2, 0, BUS_RSP_136);
    assert_false(r.timeout);
    r1(3, EMMC_RCA << 16);
    r = cmd(9, EMMC_RCA << 16, BUS_RSP_136);
    assert_int_equal(r.r[3] >> 30, 3); /* CSD_STRUCTURE: in EXT_CSD */
    set_bus(26000000, 1, false);
    r1(7, EMMC_RCA << 16);
    assert_int_equal(emmc_model_state(), EMMC_TRAN);
}

static void emmc_switch(unsigned int index, unsigned int value)
{
    r1b(6, SWITCH_WRITE(index, value));
    assert_int_equal(r1(13, EMMC_RCA << 16) & (1u << 7), 0); /* SWITCH_ERROR */
}

static void test_emmc_ddr52(void **state)
{
    (void)state;
    emmc_identify();

    /* 6.6.23: HS first, then the DDR bus width */
    emmc_switch(185, 1);
    set_bus(52000000, 1, false);
    emmc_switch(183, 6);
    set_bus(52000000, 8, true);

    r1(17, 100);
    read_blocks(512, 1);
    r1(23, 8);
    r1(25, 200);
    write_blocks(8);
    assert_int_equal(R1_STATE(r1(13, EMMC_RCA << 16)), 4);
    assert_bench_clean();
}

/* 6.6.2.2 and 6.6.2.3: HS200, tuning, back to HS, DDR 8 bit, HS400 */
static void test_emmc_hs400(void **state)
{
    (void)state;
    emmc_identify();

    r1(8, 0);
    read_blocks(512, 1);
    assert_true(buf[196] & (1u << 6)); /* HS400 at 1.8 V */

    emmc_switch(183, 2);
    set_bus(26000000, 8, false);
    emmc_switch(185, 2);
    set_bus(200000000, 8, false);
    for (int i = 0; i < 10; i++) {
        r1(21, 0);
        read_blocks(128, 1);
    }
    assert_int_equal(buf[0], 0xff);
    assert_int_equal(buf[2], 0x00);

    r1b(6, SWITCH_WRITE(185, 1));
    set_bus(52000000, 8, false);
    assert_int_equal(r1(13, EMMC_RCA << 16) & (1u << 7), 0);
    emmc_switch(183, 6);
    set_bus(52000000, 8, true);
    emmc_switch(185, 3);
    set_bus(200000000, 8, true);

    r1(18, 0);
    read_blocks(512, 4);
    r1(12, 0);
    assert_bench_clean();
}

/* Known mistakes are reported */
static void test_emmc_mistakes(void **state)
{
    (void)state;
    emmc_identify();

    /* DDR bus width before HS timing: SWITCH_ERROR (7.4.67) */
    r1b(6, SWITCH_WRITE(183, 6));
    assert_true(r1(13, EMMC_RCA << 16) & (1u << 7));
    assert_int_equal(bench_violations_of(JESD84 "7.4.67"), 1);

    /* R1b expected as R1 */
    cmd(6, SWITCH_WRITE(185, 1), BUS_RSP_48);
    assert_int_equal(bench_violations_of(JESD84 "6.10.4"), 1);
    wait_busy();

    /* Data on the wrong bus width */
    r1(17, 0);
    set_bus(52000000, 4, false);
    assert_int_equal(card->ops->read(card, buf, 512), BUS_DATA_CRC);
    assert_int_equal(bench_violations_of(JESD84 "6.6.6"), 1);

    /* Above 52 MHz in HS */
    set_bus(100000000, 1, false);
    r1(13, EMMC_RCA << 16);
    assert_int_equal(bench_violations_of(JESD84 "6.6.2"), 1);
    set_bus(52000000, 1, false);

    /* CMD21 outside HS200 */
    assert_true(cmd(21, 0, BUS_RSP_48).timeout);
    assert_int_equal(bench_violations_of(JESD84 "6.6.5.1"), 1);

    /* CMD12 after a counted read */
    r1(23, 1);
    r1(18, 0);
    read_blocks(512, 1);
    assert_true(cmd(12, 0, BUS_RSP_48).timeout);
    assert_int_equal(bench_violations_of(JESD84 "6.6.7.1"), 1);

    /* An OTP field */
    r1b(6, SWITCH_WRITE(162, 1));
    assert_int_equal(bench_violations_of(JESD84 "7.4"), 1);
}

/* SD --------------------------------------------------------------------- */

static uint16_t sd_rca;

static struct bus_rsp acmd(uint8_t idx, uint32_t arg, enum bus_rsp_type type)
{
    assert_true(r1(55, (uint32_t)sd_rca << 16) & (1u << 5)); /* APP_CMD */
    return cmd(idx, arg, type);
}

/* CMD0, CMD8, ACMD41 until ready (with S18R if asked), CMD11, CMD2, CMD3, CMD9, CMD7 */
static void sd_identify(bool s18r)
{
    struct sd_model_config cfg;
    struct bus_rsp r;

    sd_model_default_config(&cfg);
    card = sd_model_init(&cfg);
    bus.vccq_mv = 3300;
    set_bus(400000, 1, false);
    sd_rca = 0;

    cmd(0, 0, BUS_RSP_NONE);
    r = cmd(8, 0x1aa, BUS_RSP_48);
    assert_false(r.timeout);
    assert_int_equal(r.r[0] & 0xfff, 0x1aa);

    do {
        r = acmd(41, (1u << 30) | (s18r ? 1u << 24 : 0) | 0x00ff8000, BUS_RSP_48);
        assert_false(r.timeout);
        bench_advance_us(1000);
    } while (!(r.r[0] & (1u << 31)));
    assert_true(r.r[0] & (1u << 30)); /* CCS */

    if (s18r) {
        assert_true(r.r[0] & (1u << 24)); /* S18A */
        r1(11, 0);
        assert_true(card->ops->busy(card));
        /* Clock stopped, signal voltage switched, at least 5 ms */
        set_bus(0, 1, false);
        bus.vccq_mv = 1800;
        set_bus(0, 1, false);
        bench_advance_us(5000);
        set_bus(400000, 1, false);
        assert_false(card->ops->busy(card));
        assert_true(sd_model_1v8());
    }

    r = cmd(2, 0, BUS_RSP_136);
    assert_false(r.timeout);
    r = cmd(3, 0, BUS_RSP_48);
    assert_false(r.timeout);
    sd_rca = r.r[0] >> 16;
    assert_int_not_equal(sd_rca, 0);
    r = cmd(9, (uint32_t)sd_rca << 16, BUS_RSP_136);
    assert_int_equal(r.r[3] >> 30, 1); /* CSD 2.0 */
    set_bus(25000000, 1, false);
    r1b(7, (uint32_t)sd_rca << 16);
    assert_int_equal(sd_model_state(), SD_TRAN);
}

static void sd_4bit(void)
{
    struct bus_rsp r = acmd(51, 0, BUS_RSP_48);

    assert_false(r.timeout);
    read_blocks(8, 1);
    assert_int_equal(buf[1] & 0x0f, 0x05); /* SD_BUS_WIDTHS */
    acmd(6, 2, BUS_RSP_48);
    set_bus(bus.clk_hz, 4, false);
}

static void sd_switch(unsigned int fn, bool set)
{
    r1(6, (set ? 1u << 31 : 0) | 0x00fffff0 | fn);
    read_blocks(64, 1);
    assert_int_equal(buf[16] & 0x0f, fn);
}

static void test_sd_high_speed(void **state)
{
    (void)state;
    sd_identify(false);
    sd_4bit();

    sd_switch(1, false);
    sd_switch(1, true);
    set_bus(50000000, 4, false);
    assert_int_equal(sd_model_speed(), SD_HIGH_SPEED);

    memset(buf, 0x5a, 4096);
    r1(23, 8);
    r1(25, 64);
    write_blocks(8);
    r1(18, 64);
    read_blocks(512, 8);
    r1b(12, 0);
    assert_int_equal(buf[4095], 0x5a);
    assert_bench_clean();
}

static void test_sd_sdr104(void **state)
{
    (void)state;
    sd_identify(true);
    sd_4bit();

    sd_switch(3, true);
    set_bus(208000000, 4, false);
    for (int i = 0; i < 40; i++) {
        r1(19, 0);
        read_blocks(64, 1);
    }
    r1(17, 0);
    read_blocks(512, 1);
    assert_bench_clean();
}

static void test_sd_mistakes(void **state)
{
    struct sd_model_config cfg;
    struct bus_rsp r;
    (void)state;

    /* SDHC without CMD8: never ready */
    sd_model_default_config(&cfg);
    card = sd_model_init(&cfg);
    set_bus(400000, 1, false);
    sd_rca = 0;
    cmd(0, 0, BUS_RSP_NONE);
    for (int i = 0; i < 20; i++) {
        r = acmd(41, 0x00ff8000, BUS_RSP_48);
        assert_false(r.r[0] & (1u << 31));
    }

    sd_identify(false);
    /* High speed clock in default speed */
    set_bus(50000000, 1, false);
    r1(13, (uint32_t)sd_rca << 16);
    assert_int_equal(bench_violations_of(SD_PLS "CMD6"), 1);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_emmc_ddr52, setup, teardown),
        cmocka_unit_test_setup_teardown(test_emmc_hs400, setup, teardown),
        cmocka_unit_test_setup_teardown(test_emmc_mistakes, setup, teardown),
        cmocka_unit_test_setup_teardown(test_sd_high_speed, setup, teardown),
        cmocka_unit_test_setup_teardown(test_sd_sdr104, setup, teardown),
        cmocka_unit_test_setup_teardown(test_sd_mistakes, setup, teardown),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
