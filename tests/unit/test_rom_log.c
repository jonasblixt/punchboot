/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX6UL ROM log decoding (src/plat/imx6ul/rom_log.c). Saving the log reads
 * the ROM and OCRAM, so only the decoder and the state before a save are
 * tested.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <plat/imx6ul/rom_log.h>
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

    assert_int_equal(imx6ul_rom_log_decode(jiffy_log, ARRAY_SIZE(jiffy_log), 0x00901d14, out, &len),
                     PB_OK);
    assert_int_equal(len, strlen(jiffy_text));
    assert_string_equal(out, jiffy_text);
}

static void test_jiffy_uuu_log(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(
        imx6ul_rom_log_decode(jiffy_uuu_log, ARRAY_SIZE(jiffy_uuu_log), 0x00901d14, out, &len),
        PB_OK);
    assert_string_equal(out, jiffy_uuu_text);
    assert_true(imx6ul_rom_log_find(jiffy_uuu_log, ARRAY_SIZE(jiffy_uuu_log), 0x00010001));
    assert_false(imx6ul_rom_log_find(jiffy_uuu_log, ARRAY_SIZE(jiffy_uuu_log), 0x80001000));
}

static void test_stops_at_end_of_words(void **state)
{
    /* No zero word: decoding stops after n_words */
    static const uint32_t log[] = { 0x00010002, 0x000200f0 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx6ul_rom_log_decode(log, ARRAY_SIZE(log), 0x00900000, out, &len), PB_OK);
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

    assert_int_equal(imx6ul_rom_log_decode(log, ARRAY_SIZE(log), 0x00900000, out, &len), PB_OK);
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

    assert_int_equal(imx6ul_rom_log_decode(log, ARRAY_SIZE(log), 0x00900000, out, &len), PB_OK);
    assert_string_equal(out,
                        "ROM log at 0x00900000\n"
                        " 0: 00123456           Unknown event\n"
                        " 1: 01010001           Boot mode: serial download\n");
    assert_true(imx6ul_rom_log_find(log, ARRAY_SIZE(log), 0x00010001));
}

static void test_auth_failure(void **state)
{
    static const uint32_t log[] = { 0x00090000, 0x00000033, 0 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx6ul_rom_log_decode(log, ARRAY_SIZE(log), 0x00900000, out, &len), PB_OK);
    assert_non_null(strstr(out, "Authentication: fail\n"));
}

static void test_empty_log(void **state)
{
    static const uint32_t log[] = { 0 };
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx6ul_rom_log_decode(log, ARRAY_SIZE(log), 0x00900000, out, &len), PB_OK);
    assert_string_equal(out, "ROM log at 0x00900000\n");
}

static void test_output_too_small(void **state)
{
    size_t len;
    (void)state;

    /* Not room for one line */
    len = 127;
    assert_int_equal(imx6ul_rom_log_decode(jiffy_log, ARRAY_SIZE(jiffy_log), 0x00901d14, out, &len),
                     -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(len, 0);

    /* Room for the header and some lines: what fits is returned */
    memset(out, 0xaa, sizeof(out));
    len = 300;
    assert_int_equal(imx6ul_rom_log_decode(jiffy_log, ARRAY_SIZE(jiffy_log), 0x00901d14, out, &len),
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

    assert_true(imx6ul_rom_log_find(jiffy_log, ARRAY_SIZE(jiffy_log), 0x00060001));
    assert_true(imx6ul_rom_log_find(jiffy_log, ARRAY_SIZE(jiffy_log), 0x000b0000));
    assert_false(imx6ul_rom_log_find(jiffy_log, ARRAY_SIZE(jiffy_log), 0x00010001));

    /* Parameter words are not events: 0x000000f0 and 0x00000008 are parameters */
    assert_false(imx6ul_rom_log_find(jiffy_log, ARRAY_SIZE(jiffy_log), 0x000000f0));
    assert_false(imx6ul_rom_log_find(jiffy_log, ARRAY_SIZE(jiffy_log), 0x00000008));
    assert_false(imx6ul_rom_log_find(log, ARRAY_SIZE(log), 0x00060001));

    /* The log ends at the first zero event */
    assert_false(imx6ul_rom_log_find(log, ARRAY_SIZE(log), 0x00070033));
    assert_true(imx6ul_rom_log_find(log, 1, 0x00010000));
    assert_false(imx6ul_rom_log_find(log, 0, 0x00010000));
}

/* Nothing saved: no imx6ul_rom_log_save(), or a bad log pointer */
static void test_no_saved_log(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx6ul_rom_log_dump(out, &len), -PB_ERR_NOT_FOUND);
    assert_int_equal(len, 0);
    assert_false(imx6ul_rom_log_has_event(IMX6UL_ROM_LOG_PRIMARY_IMAGE));
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
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
