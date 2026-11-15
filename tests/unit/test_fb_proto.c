/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M SPL's fastboot (D-089, plat-imx.md 2), its commands: what a
 * host sends is bytes of a length that nothing has checked (security.md 1).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <string.h>

#include "../../src/plat/imx8m/spl/fb_proto.h"

#define ROOM 0x200000u

static struct fb_state s;
/* A reply is at most FB_REPLY_MAX bytes and its NUL: what is behind them
 * stays as it was */
static char reply[FB_REPLY_MAX + 1 + 8];

static int setup(void **state)
{
    (void)state;
    memset(&s, 0, sizeof(s));
    s.room = ROOM;
    memset(reply, 0x7e, sizeof(reply));
    return 0;
}

/* The command as the host sent it: its bytes, no terminator */
static enum fb_next command(const char *text)
{
    char cmd[256];
    size_t len = strlen(text);
    enum fb_next next;

    assert_true(len < sizeof(cmd));
    memset(cmd, 0x7e, sizeof(cmd));
    memcpy(cmd, text, len);
    next = fb_command(&s, cmd, len, reply);

    assert_true(strlen(reply) <= FB_REPLY_MAX);

    for (size_t i = FB_REPLY_MAX + 1; i < sizeof(reply); i++)
        assert_int_equal(reply[i], 0x7e);

    return next;
}

static void test_getvar(void **state)
{
    (void)state;

    assert_int_equal(command("getvar:version"), FB_NEXT_REPLY);
    assert_string_equal(reply, "OKAY0.4");
    assert_int_equal(command("getvar:product"), FB_NEXT_REPLY);
    assert_string_equal(reply, "OKAYpunchboot-spl");
    assert_int_equal(command("getvar:max-download-size"), FB_NEXT_REPLY);
    assert_string_equal(reply, "OKAY0x00200000");
}

/* Nothing of the device before the gate (security.md 4.7): no version of
 * pb's, no serial number, no variable the SPL doesn't name */
static void test_getvar_unknown(void **state)
{
    static const char *const names[] = {
        "getvar:",       "getvar:version-bootloader", "getvar:serialno",
        "getvar:secure", "getvar:versionx",           "getvar:versio",
        "getvar:all",
    };
    (void)state;

    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        assert_int_equal(command(names[i]), FB_NEXT_REPLY);
        assert_string_equal(reply, "FAILunknown variable");
    }
}

static void test_download(void **state)
{
    (void)state;

    assert_int_equal(command("download:00012345"), FB_NEXT_DATA);
    assert_string_equal(reply, "DATA00012345");
    assert_int_equal(s.expect, 0x12345);
    assert_int_equal(s.downloaded, 0);

    fb_data_done(&s, 1, reply);
    assert_string_equal(reply, "OKAY");
    assert_int_equal(s.expect, 0);
    assert_int_equal(s.downloaded, 0x12345);

    /* Fewer digits, and upper case */
    assert_int_equal(command("download:1F"), FB_NEXT_DATA);
    assert_string_equal(reply, "DATA0000001f");
    assert_int_equal(s.expect, 0x1f);
}

static void test_download_room(void **state)
{
    (void)state;

    assert_int_equal(command("download:00200000"), FB_NEXT_DATA);
    assert_int_equal(s.expect, ROOM);

    assert_int_equal(command("download:00200001"), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILtoo large");
    assert_int_equal(s.expect, 0);

    assert_int_equal(command("download:ffffffff"), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILtoo large");
}

static void test_download_bad_size(void **state)
{
    static const char *const sizes[] = {
        "download:",     "download:0",  "download:00000000", "download:123456789", "download:0x100",
        "download:12 4", "download:-1", "download:1g",       "download:00000100 ",
    };
    (void)state;

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        s.expect = 5;
        assert_int_equal(command(sizes[i]), FB_NEXT_REPLY);
        assert_string_equal(reply, "FAILbad size");
        assert_int_equal(s.expect, 0);
    }
}

/* A download that failed, and one that was refused, leave nothing to boot */
static void test_download_replaces(void **state)
{
    (void)state;

    assert_int_equal(command("download:100"), FB_NEXT_DATA);
    fb_data_done(&s, 1, reply);
    assert_int_equal(s.downloaded, 0x100);

    assert_int_equal(command("download:200"), FB_NEXT_DATA);
    assert_int_equal(s.downloaded, 0);
    fb_data_done(&s, 0, reply);
    assert_string_equal(reply, "FAILdownload failed");
    assert_int_equal(s.downloaded, 0);
    assert_int_equal(command("boot"), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILnothing downloaded");

    assert_int_equal(command("download:100"), FB_NEXT_DATA);
    fb_data_done(&s, 1, reply);
    assert_int_equal(command("download:ffffffff"), FB_NEXT_REPLY);
    assert_int_equal(s.downloaded, 0);

    /* Data that no download announced */
    fb_data_done(&s, 1, reply);
    assert_string_equal(reply, "FAILdownload failed");
    assert_int_equal(s.downloaded, 0);
}

static void test_boot(void **state)
{
    (void)state;

    assert_int_equal(command("boot"), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILnothing downloaded");
    assert_int_equal(command("continue"), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILnothing downloaded");

    assert_int_equal(command("download:400"), FB_NEXT_DATA);
    fb_data_done(&s, 1, reply);

    assert_int_equal(command("boot"), FB_NEXT_BOOT);
    assert_string_equal(reply, "");
    assert_int_equal(command("continue"), FB_NEXT_BOOT);
    assert_int_equal(s.downloaded, 0x400);

    /* uuu's "FB: boot" and "FB: continue": the ':' of an argument, and none */
    assert_int_equal(command("boot:"), FB_NEXT_BOOT);
    assert_int_equal(command("continue:"), FB_NEXT_BOOT);
}

static void test_unknown(void **state)
{
    static const char *const cmds[] = {
        "",        "boo",          "boot ",         "bootx",  "Boot",         "reboot",
        "flash:a", "erase:a",      "oem unlock",    "getvar", "download",     "UCmd:reset",
        "upload",  "set_active:a", "flashing lock", "\x01",   "continue:now", "boot::",
        ":",       "boot:a",       "reboot:",
    };
    (void)state;

    s.downloaded = 0x400;

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        assert_int_equal(command(cmds[i]), FB_NEXT_REPLY);
        assert_string_equal(reply, "FAILunknown command");
    }

    assert_int_equal(s.downloaded, 0x400);
}

/* A command of the longest length is read, one byte more is refused before
 * anything of it is */
static void test_length(void **state)
{
    char cmd[FB_CMD_MAX + 2];
    (void)state;

    memset(cmd, 'a', sizeof(cmd));
    memcpy(cmd, "getvar:", 7);
    cmd[FB_CMD_MAX] = '\0';
    assert_int_equal(command(cmd), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILunknown variable");

    cmd[FB_CMD_MAX] = 'a';
    cmd[FB_CMD_MAX + 1] = '\0';
    assert_int_equal(command(cmd), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILcommand too long");

    /* A NUL inside a command is a byte as any other */
    assert_int_equal(fb_command(&s, "boot\0", 5, reply), FB_NEXT_REPLY);
    assert_string_equal(reply, "FAILunknown command");
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_getvar, setup),
        cmocka_unit_test_setup(test_getvar_unknown, setup),
        cmocka_unit_test_setup(test_download, setup),
        cmocka_unit_test_setup(test_download_room, setup),
        cmocka_unit_test_setup(test_download_bad_size, setup),
        cmocka_unit_test_setup(test_download_replaces, setup),
        cmocka_unit_test_setup(test_boot, setup),
        cmocka_unit_test_setup(test_unknown, setup),
        cmocka_unit_test_setup(test_length, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
