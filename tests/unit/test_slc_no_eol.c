/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The security life cycle (src/slc.c) in a build without CONFIG_SLC_EOL,
 * the default (system.md 4, F-263): the device can't enter EOL. With the
 * option, which the host's configuration has: test_rot.c.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <pb/slc.h>

#ifdef CONFIG_SLC_EOL
#error "This test is built without CONFIG_SLC_EOL"
#endif

static slc_t slc_state;
static int slc_writes;
static int eol_calls;

static slc_t fake_read_status(void)
{
    return slc_state;
}

static int fake_set_configuration(void)
{
    slc_writes++;
    slc_state = SLC_CONFIGURATION;
    return PB_OK;
}

static int fake_set_configuration_locked(void)
{
    slc_writes++;
    slc_state = SLC_CONFIGURATION_LOCKED;
    return PB_OK;
}

static int fake_set_eol(void)
{
    eol_calls++;
    slc_state = SLC_EOL;
    return PB_OK;
}

/* A port that has EOL */
static const struct slc_config cfg = {
    .read_status = fake_read_status,
    .set_configuration = fake_set_configuration,
    .set_configuration_locked = fake_set_configuration_locked,
    .set_eol = fake_set_eol,
};

/* Runs first: slc.c keeps its configuration in a static */
static void test_not_initialized(void **state)
{
    (void)state;

    assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
}

/* From no state does the device enter EOL: the port's set_eol isn't called,
 * and the answer is the one of a port without it, NOT_SUPPORTED on the wire
 * (command-mode.md 5.2) */
static void test_eol_is_left_out(void **state)
{
    static const slc_t states[] = {
        SLC_NOT_CONFIGURED,
        SLC_CONFIGURATION,
        SLC_CONFIGURATION_LOCKED,
        /* In EOL already, from a build that had the option: also not OK */
        SLC_EOL,
        /* A life cycle that can't be read: the build's answer, not the read's */
        -PB_ERR_IO,
    };
    (void)state;

    assert_int_equal(slc_init(&cfg), PB_OK);
    eol_calls = 0;

    for (size_t i = 0; i < ARRAY_SIZE(states); i++) {
        slc_state = states[i];
        assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
        assert_int_equal(slc_read_status(), states[i]);
    }

    assert_int_equal(eol_calls, 0);
}

/* The rest of the life cycle is as with the option */
static void test_the_other_states(void **state)
{
    (void)state;

    assert_int_equal(slc_init(&cfg), PB_OK);
    slc_state = SLC_NOT_CONFIGURED;
    slc_writes = 0;
    eol_calls = 0;

    assert_int_equal(slc_set_configuration(), PB_OK);
    assert_int_equal(slc_read_status(), SLC_CONFIGURATION);
    assert_int_equal(slc_set_configuration_locked(), PB_OK);
    assert_int_equal(slc_read_status(), SLC_CONFIGURATION_LOCKED);
    assert_int_equal(slc_writes, 2);

    /* A device that is in EOL reads as that, and stays behind the order */
    slc_state = SLC_EOL;
    assert_int_equal(slc_read_status(), SLC_EOL);
    assert_int_equal(slc_set_configuration(), -PB_ERR_STATE);
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_STATE);
    assert_int_equal(slc_writes, 2);
    assert_int_equal(eol_calls, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_not_initialized),
        cmocka_unit_test(test_eol_is_left_out),
        cmocka_unit_test(test_the_other_states),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
