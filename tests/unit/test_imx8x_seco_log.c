/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8X's SECO log (src/plat/imx8x/seco_log.c), with the SCFW's two
 * calls faked.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/crc.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <plat/imx8x/seco_log.h>
#include <string.h>

/* An unsigned container on an open part (U-Boot's AHAB guide shows it), a
 * failed request, and codes that have no name */
static const uint32_t fake_events[] = { 0x0087ee00, 0x00a2b629, 0x00b2b201 };

static size_t n_fake_events;
static sc_err_t chip_info_err;
static unsigned int get_event_calls;

sc_err_t
sc_seco_chip_info(sc_ipc_t ipc, uint16_t *lc, uint16_t *monotonic, uint32_t *uid_l, uint32_t *uid_h)
{
    (void)ipc;
    assert_null(monotonic);
    assert_null(uid_l);
    assert_null(uid_h);
    *lc = 0x0008;
    return chip_info_err;
}

sc_err_t sc_seco_get_event(sc_ipc_t ipc, uint8_t idx, uint32_t *event)
{
    (void)ipc;
    get_event_calls++;
    if (idx >= n_fake_events)
        return SC_ERR_PARM;
    *event = fake_events[idx];
    return SC_ERR_NONE;
}

static char out[4096];

static int setup(void **state)
{
    (void)state;
    n_fake_events = 0;
    chip_info_err = SC_ERR_NONE;
    get_event_calls = 0;
    memset(out, 0, sizeof(out));
    return 0;
}

static void test_decode_events(void **state)
{
    static const char text[] = "SECO life cycle: open (0x0008)\n"
                               "Event 0: 0x0087ee00, command: authenticate container (0x87), "
                               "indication: no authentication (0xee), status: success (0x00)\n"
                               "Event 1: 0x00a2b629, command: get event (0xa2), "
                               "indication: disabled feature (0xb6), status: failure (0x29)\n"
                               "Event 2: 0x00b2b201, command: unknown (0xb2), "
                               "indication: unknown (0xb2), status: unknown (0x01)\n";
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx8x_seco_log_decode(0x0008, fake_events, 3, out, &len), PB_OK);
    assert_int_equal(len, strlen(text));
    assert_memory_equal(out, text, len);
}

static void test_decode_no_events(void **state)
{
    static const char text[] = "SECO life cycle: OEM closed (0x0080)\n"
                               "No SECO events\n";
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx8x_seco_log_decode(0x0080, NULL, 0, out, &len), PB_OK);
    assert_int_equal(len, strlen(text));
    assert_memory_equal(out, text, len);
}

static void test_decode_life_cycle_not_read(void **state)
{
    static const char text[] = "SECO life cycle: not read (-3)\n"
                               "No SECO events\n";
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx8x_seco_log_decode(-3, NULL, 0, out, &len), PB_OK);
    assert_int_equal(len, strlen(text));
    assert_memory_equal(out, text, len);
}

static void test_decode_unknown_life_cycle(void **state)
{
    static const char text[] = "SECO life cycle: unknown (0x0040)\n";
    size_t len = sizeof(out);
    (void)state;

    assert_int_equal(imx8x_seco_log_decode(0x0040, NULL, 0, out, &len), PB_OK);
    assert_true(len > strlen(text));
    assert_memory_equal(out, text, strlen(text));
}

static void test_decode_truncated(void **state)
{
    /* Whole lines only: the first event's line doesn't fit */
    static const char text[] = "SECO life cycle: open (0x0008)\n";
    size_t len = sizeof(text) + 10;
    (void)state;

    assert_int_equal(imx8x_seco_log_decode(0x0008, fake_events, 2, out, &len),
                     -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(len, strlen(text));
    assert_string_equal(out, text);
}

static void test_decode_no_room(void **state)
{
    size_t len = 0;
    (void)state;

    assert_int_equal(imx8x_seco_log_decode(0x0008, NULL, 0, out, &len), -PB_ERR_BUF_TOO_SMALL);
}

/* pb_id() of a log's name, as the host sends it */
static uint32_t log_id(const char *name)
{
    return crc32(0, (const uint8_t *)name, (uint32_t)strlen(name));
}

static void test_log_source(void **state)
{
    const struct log_source *src;
    size_t len;
    (void)state;

    n_fake_events = 3;
    assert_int_equal(imx8x_seco_log_init(0), PB_OK);
    /* Nothing is asked before the first read */
    assert_int_equal(get_event_calls, 0);

    src = log_source_get(log_id("seco"));
    assert_non_null(src);
    assert_string_equal(src->name, "seco");

    len = src->length();
    assert_int_equal(src->head(), len);
    /* The three events and the call that ended them */
    assert_int_equal(get_event_calls, 4);
    assert_int_equal(src->read(0, out, sizeof(out)), len);
    assert_non_null(strstr(out, "SECO life cycle: open (0x0008)\nEvent 0: 0x0087ee00"));

    /* Read once: the SCFW isn't asked again */
    n_fake_events = 0;
    assert_int_equal(src->length(), len);
    assert_int_equal(get_event_calls, 4);
    assert_int_equal(src->read(len, out, sizeof(out)), 0);
    assert_int_equal(src->read(len - 3, out, sizeof(out)), 3);
}

static void test_log_source_chip_info_fails(void **state)
{
    const struct log_source *src;
    (void)state;

    chip_info_err = SC_ERR_UNAVAILABLE;
    /* Again: the text is made again at the next read */
    assert_int_equal(imx8x_seco_log_init(0), PB_OK);
    src = log_source_get(log_id("seco"));
    assert_non_null(src);
    assert_true(src->length() > 0);
    assert_int_equal(src->read(0, out, sizeof(out)), src->length());
    assert_non_null(strstr(out, "not read (-"));
    assert_non_null(strstr(out, "No SECO events\n"));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_decode_events, setup),
        cmocka_unit_test_setup(test_decode_no_events, setup),
        cmocka_unit_test_setup(test_decode_life_cycle_not_read, setup),
        cmocka_unit_test_setup(test_decode_unknown_life_cycle, setup),
        cmocka_unit_test_setup(test_decode_truncated, setup),
        cmocka_unit_test_setup(test_decode_no_room, setup),
        cmocka_unit_test_setup(test_log_source, setup),
        cmocka_unit_test_setup(test_log_source_chip_info_fails, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
