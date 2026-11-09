/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HAB event log (src/drivers/rom_log/imx_hab_log.c), with fake HAB API
 * functions in a fake RVT.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/rom_log/imx_hab_log.h>
#include <pb/crc.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <string.h>

/* An unsigned image on an open part: authentication fails, it boots anyway */
static const uint8_t event0[] = {
    0xdb, 0x00, 0x14, 0x43, 0x33, 0x18, 0x0a, 0x1d, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x92, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
};

static uint32_t fake_status = 0x33;
static size_t fake_events = 1;

static uint32_t fake_report_status(uint32_t *config, uint32_t *state)
{
    *config = 0xf0;
    *state = 0x66;
    return fake_status;
}

static uint32_t fake_report_event(uint32_t status, uint32_t index, uint8_t *event, size_t *bytes)
{
    assert_int_equal(status, 0); /* Any status */
    if (index >= fake_events || *bytes < sizeof(event0))
        return 0x33;
    memcpy(event, event0, sizeof(event0));
    *bytes = sizeof(event0);
    return 0xf0;
}

static const char event_text[] = "HAB status: failure (0x33), config: open (0xf0), state: "
                                 "non-secure (0x66)\n"
                                 "Event 0: status: failure (0x33), reason: invalid signature "
                                 "(0x18), context: authenticate "
                                 "image (0x0a), engine: CAAM (0x1d)\n"
                                 "  db 00 14 43 33 18 0a 1d 00 00 00 00 00 92 00 00\n"
                                 "  00 00 04 00\n";

static char out[4096];

static void test_decode(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx_hab_log_decode(fake_report_status, fake_report_event, out, &len), PB_OK);
    assert_string_equal(out, event_text);
    assert_int_equal(len, strlen(event_text));
}

static void test_no_events(void **state)
{
    size_t len = sizeof(out);
    (void)state;

    fake_status = 0xf0;
    fake_events = 0;
    assert_int_equal(imx_hab_log_decode(fake_report_status, fake_report_event, out, &len), PB_OK);
    assert_string_equal(out,
                        "HAB status: success (0xf0), config: open (0xf0), state: non-secure "
                        "(0x66)\n");
    fake_status = 0x33;
    fake_events = 1;
}

static void test_truncated(void **state)
{
    size_t len = 40;
    (void)state;

    assert_int_equal(imx_hab_log_decode(fake_report_status, fake_report_event, out, &len),
                     -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(len, 39);
    assert_int_equal(strncmp(out, event_text, 39), 0);
}

/* An RVT on a 64-bit host: header, then 8 byte pointers from offset 8 */
static uint64_t fake_rvt[16];

static const struct log_source *hab_source(void)
{
    return log_source_get(crc32(0, (const uint8_t *)"hab", 3));
}

static void test_log_source(void **state)
{
    static const struct imx_hab_log_config bad = { .rvt = (uintptr_t)&fake_rvt[15],
                                                   .entry_size = 8 };
    static const struct imx_hab_log_config cfg = { .rvt = (uintptr_t)fake_rvt, .entry_size = 8 };
    char text[sizeof(event_text)];
    const struct log_source *src;
    (void)state;

    /* No RVT header: no log */
    assert_int_equal(imx_hab_log_init(&bad), -PB_ERR_NOT_FOUND);
    assert_null(hab_source());

    fake_rvt[0] = 0x43006000dd; /* Tag 0xdd */
    fake_rvt[1 + 7] = (uintptr_t)fake_report_event;
    fake_rvt[1 + 8] = (uintptr_t)fake_report_status;
    assert_int_equal(imx_hab_log_init(&cfg), PB_OK);
    src = hab_source();
    assert_non_null(src);
    assert_int_equal(src->head(), strlen(event_text));
    assert_int_equal(src->length(), strlen(event_text));
    assert_int_equal(src->read(0, text, sizeof(text)), strlen(event_text));
    assert_memory_equal(text, event_text, strlen(event_text));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_decode),
        cmocka_unit_test(test_no_events),
        cmocka_unit_test(test_truncated),
        cmocka_unit_test(test_log_source),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
