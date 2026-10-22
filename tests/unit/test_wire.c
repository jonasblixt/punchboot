/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Wire protocol helpers (src/wire.c), shared by the device and the host tools.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb-tools/error.h>
#include <pb-tools/wire.h>
#include <string.h>

static void test_init_command(void **state)
{
    struct pb_command cmd;
    uint8_t data[PB_COMMAND_REQUEST_MAX_SIZE];
    (void)state;

    memset(&cmd, 0xff, sizeof(cmd));
    assert_int_equal(pb_wire_init_command(&cmd, PB_CMD_SLC_READ), PB_RESULT_OK);
    assert_int_equal(cmd.magic, PB_WIRE_MAGIC);
    assert_int_equal(cmd.command, PB_CMD_SLC_READ);
    assert_int_equal(cmd.request[0], 0);
    assert_true(pb_wire_valid_command(&cmd));

    memset(data, 0x42, sizeof(data));
    assert_int_equal(pb_wire_init_command2(&cmd, PB_CMD_PART_VERIFY, data, sizeof(data)),
                     PB_RESULT_OK);
    assert_int_equal(cmd.command, PB_CMD_PART_VERIFY);
    assert_memory_equal(cmd.request, data, sizeof(data));

    assert_int_equal(pb_wire_init_command2(&cmd, PB_CMD_PART_VERIFY, data, sizeof(data) + 1),
                     -PB_RESULT_NO_MEMORY);
}

static void test_init_result(void **state)
{
    struct pb_result result;
    uint8_t data[PB_RESULT_RESPONSE_MAX_SIZE];
    (void)state;

    memset(&result, 0xff, sizeof(result));
    assert_int_equal(pb_wire_init_result(&result, -PB_RESULT_NOT_FOUND), PB_RESULT_OK);
    assert_int_equal(result.magic, PB_WIRE_MAGIC);
    assert_int_equal(result.result_code, -PB_RESULT_NOT_FOUND);
    assert_int_equal(result.response[0], 0);
    assert_true(pb_wire_valid_result(&result));

    memset(data, 0x42, sizeof(data));
    assert_int_equal(pb_wire_init_result2(&result, PB_RESULT_OK, data, sizeof(data)), PB_RESULT_OK);
    assert_memory_equal(result.response, data, sizeof(data));

    assert_int_equal(pb_wire_init_result2(&result, PB_RESULT_OK, data, sizeof(data) + 1),
                     -PB_RESULT_NO_MEMORY);

    result.magic = 0;
    assert_false(pb_wire_valid_result(&result));
}

static void test_valid_command(void **state)
{
    struct pb_command cmd;
    (void)state;

    pb_wire_init_command(&cmd, PB_CMD_INVALID);
    assert_false(pb_wire_valid_command(&cmd));

    pb_wire_init_command(&cmd, PB_CMD_END);
    assert_false(pb_wire_valid_command(&cmd));

    pb_wire_init_command(&cmd, PB_CMD_END - 1);
    assert_true(pb_wire_valid_command(&cmd));

    cmd.magic = PB_WIRE_MAGIC + 1;
    assert_false(pb_wire_valid_command(&cmd));
}

static void test_requires_auth(void **state)
{
    struct pb_command cmd;
    (void)state;

    /* Only reading the device identifier and authenticating are open */
    for (int c = PB_CMD_INVALID + 1; c < PB_CMD_END; c++) {
        pb_wire_init_command(&cmd, c);
        bool open = (c == PB_CMD_DEVICE_IDENTIFIER_READ) || (c == PB_CMD_AUTHENTICATE);
        assert_int_equal(pb_wire_requires_auth(&cmd), !open);
    }
}

static void test_strings(void **state)
{
    (void)state;

    /* Not implemented (F-032), but must return a valid string */
    assert_non_null(pb_wire_command_string(PB_CMD_SLC_READ));
    assert_non_null(pb_wire_slc_string(PB_SLC_EOL));
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_command),  cmocka_unit_test(test_init_result),
        cmocka_unit_test(test_valid_command), cmocka_unit_test(test_requires_auth),
        cmocka_unit_test(test_strings),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
