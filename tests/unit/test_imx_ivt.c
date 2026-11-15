/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M SPL's check of pb proper's header (D-088, plat-imx.md 2,
 * security.md 1): what it reads of pb proper before the ROM's HAB has
 * checked it is what the SPL was built for.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <string.h>

#include "../../src/plat/imx8m/spl/ivt.h"

#define ENTRY      0x41100000u
#define REGION_END (ENTRY + 0x200000u)
#define FLOOR      0x41060000u

static const struct imx_ivt_limits limits = {
    .entry = ENTRY,
    .region_end = REGION_END,
    .load_floor = FLOOR,
};

/* pb proper's header as imx-mkimage writes it for the pico8ml: the plugin's
 * part is loaded below the IVT, the CSF after pb.bin */
static struct imx_header good(void)
{
    struct imx_header h;

    memset(&h, 0, sizeof(h));
    h.ivt.header = 0x412000d1;
    h.ivt.entry = ENTRY;
    h.ivt.self = ENTRY - IMX_HEADER_SIZE;
    h.ivt.boot_data = h.ivt.self + 0x20;
    h.ivt.csf = 0x411387c0;
    h.boot_data.start = 0x410e99c0;
    h.boot_data.size = 0x51060;
    return h;
}

static void test_mkimage_header(void **state)
{
    struct imx_header h = good();
    (void)state;

    assert_int_equal(imx_ivt_check(&h, &limits), PB_OK);
}

static void test_ivt_header(void **state)
{
    static const uint32_t bad[] = {
        0x412000d2, /* Another tag */
        0x411f00d1, /* Another length */
        0x3f2000d1, /* Versions below 4.0 */
        0x502000d1, /* and above 4.f */
    };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
        struct imx_header h = good();
        h.ivt.header = bad[i];
        assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);
    }

    struct imx_header h = good();
    h.ivt.header = 0x402000d1;
    assert_int_equal(imx_ivt_check(&h, &limits), PB_OK);
}

/* Its entry and address are the build's PB_ENTRY's, its boot data follows it,
 * no DCD and no plugin: an image for another pb, or one that would run code
 * before HAB, is refused */
static void test_ivt_fields(void **state)
{
    struct imx_header h;
    (void)state;

    h = good();
    h.ivt.entry += 4;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.ivt.self -= 0x40;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.ivt.boot_data += 4;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.ivt.dcd = h.ivt.self + 0x40;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.boot_data.plugin = IMX_BOOT_DATA_PLUGIN;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);
}

/* The load range, which the ROM writes before HAB has checked anything:
 * after pb's buffers, from at most the IVT, inside pb proper's region; and
 * the CSF in it, after the IVT */
static void test_load_range(void **state)
{
    struct imx_header h;
    (void)state;

    h = good();
    h.boot_data.size = 0;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.boot_data.start = FLOOR - 0x200;
    h.boot_data.size += 0x200;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.boot_data.start = FLOOR;
    h.boot_data.size = REGION_END - FLOOR;
    assert_int_equal(imx_ivt_check(&h, &limits), PB_OK);

    h.boot_data.size += 1;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.boot_data.start = h.ivt.self + 0x40;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    /* A size that wraps 32 bits */
    h = good();
    h.boot_data.size = 0xffffffff;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.ivt.csf = h.ivt.self;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);

    h = good();
    h.ivt.csf = h.boot_data.start + h.boot_data.size;
    assert_int_equal(imx_ivt_check(&h, &limits), -PB_ERR_PARAM);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_mkimage_header),
        cmocka_unit_test(test_ivt_header),
        cmocka_unit_test(test_ivt_fields),
        cmocka_unit_test(test_load_range),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
