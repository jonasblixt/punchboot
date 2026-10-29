/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX ROM log decoding (src/drivers/rom_log/imx_rom_log.c). Saving the log reads
 * the ROM and OCRAM, so only the decoder and the state before a save are
 * tested.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/rom_log/imx_rom_log.h>
#include <pb/crc.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <string.h>

/* Logged by jiffy (closed, HAB) booting pb from eMMC */
static const uint32_t jiffy_log[] = {
    0x000e0000, 0x00010000, 0x000200cc, 0x00030000, 0x00040001, 0x00050000, 0x00060001,
    0x00070000, 0x000700f0, 0x00080000, 0x00000000, 0x000800f0, 0x00080000, 0x00000008,
    0x000800f0, 0x00090000, 0x000000f0, 0x000b0000, 0x80001000, 0x00000000, 0x00000000,
};

static const char jiffy_text[] = "ROM log at 0x00901d14\n"
                                 " 0: 000e0000           Internal use\n"
                                 " 1: 00010000           Boot mode: boot from fuses\n"
                                 " 2: 000200cc           Security config: closed\n"
                                 " 3: 00030000           Internal use\n"
                                 " 4: 00040001           FUSE_SEL_VALUE fuse blown\n"
                                 " 5: 00050000           Boot from the primary image\n"
                                 " 6: 00060001           Primary boot device: SD/eMMC\n"
                                 " 7: 00070000           Device init\n"
                                 " 8: 000700f0           Device init: pass\n"
                                 " 9: 00080000 00000000  Device read, image offset: 0x00000000\n"
                                 "11: 000800f0           Device read: pass\n"
                                 "12: 00080000 00000008  Device read, image offset: 0x00000008\n"
                                 "14: 000800f0           Device read: pass\n"
                                 "15: 00090000 000000f0  Authentication: pass\n"
                                 "17: 000b0000 80001000  Jump to boot image: 0x80001000\n";

/* Logged by jiffy (closed, HAB) when uuu loaded pb over USB */
static const uint32_t jiffy_uuu_log[] = {
    0x000e0000, 0x00010001, 0x000200cc, 0x00030000, 0x00040001, 0x00062001, 0x00070000,
    0x00070033, 0x000c0000, 0x00090000, 0x000000f0, 0x000d0000, 0x80001000, 0x00000000,
};

static const char jiffy_uuu_text[] = "ROM log at 0x00901d14\n"
                                     " 0: 000e0000           Internal use\n"
                                     " 1: 00010001           Boot mode: serial download\n"
                                     " 2: 000200cc           Security config: closed\n"
                                     " 3: 00030000           Internal use\n"
                                     " 4: 00040001           FUSE_SEL_VALUE fuse blown\n"
                                     " 5: 00062001           Manufacture boot device: SD/eMMC\n"
                                     " 6: 00070000           Device init\n"
                                     " 7: 00070033           Device init: fail\n"
                                     " 8: 000c0000           Serial download\n"
                                     " 9: 00090000 000000f0  Authentication: pass\n"
                                     "11: 000d0000 80001000  Jump to serial download image: "
                                     "0x80001000\n";

static char out[4096];

static void test_jiffy_log(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(
            IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x00901d14, out, &len),
        PB_OK);
    assert_int_equal(len, strlen(jiffy_text));
    assert_string_equal(out, jiffy_text);
}

static void test_jiffy_uuu_log(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(
            IMX_ROM_LOG_DEFS_V0, jiffy_uuu_log, ARRAY_SIZE(jiffy_uuu_log), 0x00901d14, out, &len),
        PB_OK);
    assert_string_equal(out, jiffy_uuu_text);
    assert_true(imx_rom_log_find(
        IMX_ROM_LOG_DEFS_V0, jiffy_uuu_log, ARRAY_SIZE(jiffy_uuu_log), 0x00010001));
    assert_false(imx_rom_log_find(
        IMX_ROM_LOG_DEFS_V0, jiffy_uuu_log, ARRAY_SIZE(jiffy_uuu_log), 0x80001000));
}

static void test_stops_at_end_of_words(void **state)
{
    /* No zero word: decoding stops after n_words */
    static const uint32_t log[] = { 0x00010002, 0x000200f0 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00900000, out, &len),
        PB_OK);
    assert_string_equal(out,
                        "ROM log at 0x00900000\n"
                        " 0: 00010002           Boot mode: internal boot\n"
                        " 1: 000200f0           Security config: open\n");
}

static void test_missing_parameter(void **state)
{
    /* The log ends where a parameter word should be */
    static const uint32_t log[] = { 0x00090000 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00900000, out, &len),
        PB_OK);
    assert_string_equal(out,
                        "ROM log at 0x00900000\n"
                        " 0: 00090000           Authentication: parameter missing\n");
}

static void test_unknown_and_reserved_bits(void **state)
{
    /* Bits 31:24 are reserved and not part of the event ID */
    static const uint32_t log[] = { 0x00123456, 0x01010001, 0 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00900000, out, &len),
        PB_OK);
    assert_string_equal(out,
                        "ROM log at 0x00900000\n"
                        " 0: 00123456           Unknown event\n"
                        " 1: 01010001           Boot mode: serial download\n");
    assert_true(imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00010001));
}

static void test_auth_failure(void **state)
{
    static const uint32_t log[] = { 0x00090000, 0x00000033, 0 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00900000, out, &len),
        PB_OK);
    assert_non_null(strstr(out, "Authentication: fail\n"));
}

static void test_empty_log(void **state)
{
    static const uint32_t log[] = { 0 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00900000, out, &len),
        PB_OK);
    assert_string_equal(out, "ROM log at 0x00900000\n");
}

static void test_output_too_small(void **state)
{
    size_t len;
    (void)state;

    /* Not room for one line */
    len = 127;
    assert_int_equal(
        imx_rom_log_decode(
            IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x00901d14, out, &len),
        -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(len, 0);

    /* Room for the header and some lines: what fits is returned */
    memset(out, 0xaa, sizeof(out));
    len = 300;
    assert_int_equal(
        imx_rom_log_decode(
            IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x00901d14, out, &len),
        -PB_ERR_BUF_TOO_SMALL);
    assert_true(len > 0 && len < 300);
    assert_int_equal(strncmp(out, jiffy_text, len), 0);
    assert_int_equal((uint8_t)out[300], 0xaa);
}

static void test_find_event(void **state)
{
    /* Device read and its parameter 0x00000008, then a zero word */
    static const uint32_t log[] = { 0x00010000, 0x00080000, 0x00060001, 0, 0x00070033 };
    (void)state;

    assert_true(
        imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x00060001));
    assert_true(
        imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x000b0000));
    assert_false(
        imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x00010001));

    /* Parameter words are not events: 0x000000f0 and 0x00000008 are parameters */
    assert_false(
        imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x000000f0));
    assert_false(
        imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), 0x00000008));
    assert_false(imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00060001));

    /* The log ends at the first zero event */
    assert_false(imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, log, ARRAY_SIZE(log), 0x00070033));
    assert_true(imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, log, 1, 0x00010000));
    assert_false(imx_rom_log_find(IMX_ROM_LOG_DEFS_V0, log, 0, 0x00010000));
}

/* Nothing saved: no imx_rom_log_save(), or a bad log pointer */
static void test_no_saved_log(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx_rom_log_dump(out, &len), -PB_ERR_NOT_FOUND);
    assert_int_equal(len, 0);
    assert_false(imx_rom_log_has_event(IMX_ROM_LOG_V0_PRIMARY_IMAGE));
}

/* An i.MX 8M log (definition version 2, IDs in bits 31:24), made from
 * AN12853 Table 9: two parameters, a version in the event word, ticks */
static void test_v2_log(void **state)
{
    static const uint32_t log[] = {
        0x01000002, 0x10000000, 0x22000000, 0x50000000, 0x61000000, 0x80000000, 1000,
        0x81000000, 1200,       0x90000000, 0x00008400, 0x91000000, 1500,       0xa0000000,
        0x000000f0, 1600,       0xc0000000, 0x00920000, 1700,       0x55000000, 0,
    };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(IMX_ROM_LOG_DEFS_V2, log, ARRAY_SIZE(log), 0x00900000, out, &len),
        PB_OK);
    assert_string_equal(out,
                        "ROM log at 0x00900000\n"
                        " 0: 01000002           ROM event version: 2\n"
                        " 1: 10000000           Boot mode: boot from fuses\n"
                        " 2: 22000000           Security config: open\n"
                        " 3: 50000000           Boot from the primary image\n"
                        " 4: 61000000           Primary boot device: SD/eMMC\n"
                        " 5: 80000000 000003e8  Device init: tick 1000\n"
                        " 7: 81000000 000004b0  Device init: pass, tick 1200\n"
                        " 9: 90000000 00008400  Device read, image offset: 0x00008400\n"
                        "11: 91000000 000005dc  Device read: pass, tick 1500\n"
                        "13: a0000000 000000f0 00000640  Authentication: pass, tick 1600\n"
                        "16: c0000000 00920000 000006a4  Jump to boot image: 0x00920000, tick "
                        "1700\n"
                        "19: 55000000           Unknown event\n");
    assert_true(imx_rom_log_find(IMX_ROM_LOG_DEFS_V2, log, ARRAY_SIZE(log), 0xc0));
    /* Parameters aren't events: 0x00920000 has 0x00 in bits 31:24, 1600 too */
    assert_false(imx_rom_log_find(IMX_ROM_LOG_DEFS_V2, log, ARRAY_SIZE(log), 0x00));
    /* Two parameters missing */
    static const uint32_t short_log[] = { 0xc0000000, 0x00920000 };
    len = sizeof(out);
    assert_int_equal(
        imx_rom_log_decode(IMX_ROM_LOG_DEFS_V2, short_log, ARRAY_SIZE(short_log), 0, out, &len),
        PB_OK);
    assert_non_null(strstr(out, " 0: c0000000           Jump to boot image: parameter missing\n"));
}

/* Logged by the i.MX8M Plus EVK (open, unsigned pb.imx) booting pb from
 * eMMC BOOT0. HAB fails the unsigned image and the open part boots it anyway */
static const uint32_t evk_8mp_log[] = {
    0xe0000000, 0x22000000, 0x31000000, 0x40000000, 0x61000000, 0x82000000, 0x00000ed7,
    0x83000000, 0x80000000, 0x00000ee1, 0x81000000, 0x00002acb, 0x50000000, 0x90000000,
    0x00000000, 0x91000000, 0x00002db0, 0x90000000, 0x00000008, 0x91000000, 0x00005805,
    0xa0000000, 0x00cf1133, 0x00005914, 0xc0000000, 0x00920000, 0x00006bff, 0x00000000,
};

static void test_8mp_evk_log(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx_rom_log_decode(
            IMX_ROM_LOG_DEFS_V2, evk_8mp_log, ARRAY_SIZE(evk_8mp_log), 0x009112f8, out, &len),
        PB_OK);
    assert_string_equal(out,
                        "ROM log at 0x009112f8\n"
                        " 0: e0000000           Internal use\n"
                        " 1: 22000000           Security config: open\n"
                        " 2: 31000000           Internal use\n"
                        " 3: 40000000           FUSE_SEL_VALUE fuse not blown\n"
                        " 4: 61000000           Primary boot device: SD/eMMC\n"
                        " 5: 82000000 00000ed7  Device pre-config: tick 3799\n"
                        " 7: 83000000           Device pre-config: pass\n"
                        " 8: 80000000 00000ee1  Device init: tick 3809\n"
                        "10: 81000000 00002acb  Device init: pass, tick 10955\n"
                        "12: 50000000           Boot from the primary image\n"
                        "13: 90000000 00000000  Device read, image offset: 0x00000000\n"
                        "15: 91000000 00002db0  Device read: pass, tick 11696\n"
                        "17: 90000000 00000008  Device read, image offset: 0x00000008\n"
                        "19: 91000000 00005805  Device read: pass, tick 22533\n"
                        "21: a0000000 00cf1133 00005914  Authentication: fail, tick 22804\n"
                        "24: c0000000 00920000 00006bff  Jump to boot image: 0x00920000, tick "
                        "27647\n");
}

/* An unknown definition version */
static void test_unknown_defs(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx_rom_log_decode(1, jiffy_log, ARRAY_SIZE(jiffy_log), 0, out, &len),
                     -PB_ERR_PARAM);
    assert_false(imx_rom_log_find(1, jiffy_log, ARRAY_SIZE(jiffy_log), 0x00010000));
}

/* A ROM pointer at a log, at addresses below 4 GiB (-no-pie) */
static uint32_t fake_rom_log[64];
static uint32_t fake_rom_ptr;

static const struct imx_rom_log_config fake_cfg = {
    .pointer = (uintptr_t)&fake_rom_ptr,
    .ram_start = (uintptr_t)fake_rom_log,
    .ram_end = (uintptr_t)fake_rom_log + sizeof(fake_rom_log),
    .words = ARRAY_SIZE(fake_rom_log),
    .defs = IMX_ROM_LOG_DEFS_V0,
};

static const struct log_source *rom_source(void)
{
    return log_source_get(crc32(0, (const uint8_t *)"rom", 3));
}

/* A saved log is the log "rom", as the text the decoder makes */
static void test_saved_log_source(void **state)
{
    static char text[4096];
    static char read[4096];
    const struct log_source *src;
    size_t len = sizeof(text);
    (void)state;

    assert_true((uintptr_t)fake_rom_log < 0x100000000ULL);

    /* A pointer outside the RAM: no log */
    fake_rom_ptr = (uint32_t)(uintptr_t)fake_rom_log + sizeof(fake_rom_log);
    imx_rom_log_save(&fake_cfg);
    assert_null(rom_source());
    assert_false(imx_rom_log_has_event(IMX_ROM_LOG_V0_JUMP));

    memcpy(fake_rom_log, jiffy_log, sizeof(jiffy_log));
    fake_rom_ptr = (uint32_t)(uintptr_t)fake_rom_log;
    imx_rom_log_save(&fake_cfg);
    assert_true(imx_rom_log_has_event(IMX_ROM_LOG_V0_JUMP));

    assert_int_equal(
        imx_rom_log_decode(
            IMX_ROM_LOG_DEFS_V0, jiffy_log, ARRAY_SIZE(jiffy_log), fake_rom_ptr, text, &len),
        PB_OK);
    src = rom_source();
    assert_non_null(src);
    assert_int_equal(src->head(), len);
    assert_int_equal(src->length(), len);
    assert_int_equal(src->read(0, read, sizeof(read)), len);
    assert_memory_equal(read, text, len);
    assert_int_equal(src->read(len - 5, read, sizeof(read)), 5);
    assert_memory_equal(read, &text[len - 5], 5);
    assert_int_equal(src->read(len, read, sizeof(read)), 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_jiffy_log),
        cmocka_unit_test(test_jiffy_uuu_log),
        cmocka_unit_test(test_stops_at_end_of_words),
        cmocka_unit_test(test_missing_parameter),
        cmocka_unit_test(test_unknown_and_reserved_bits),
        cmocka_unit_test(test_auth_failure),
        cmocka_unit_test(test_empty_log),
        cmocka_unit_test(test_output_too_small),
        cmocka_unit_test(test_find_event),
        cmocka_unit_test(test_no_saved_log),
        cmocka_unit_test(test_saved_log_source),
        cmocka_unit_test(test_v2_log),
        cmocka_unit_test(test_8mp_evk_log),
        cmocka_unit_test(test_unknown_defs),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
