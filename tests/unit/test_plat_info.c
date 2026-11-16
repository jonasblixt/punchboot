/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * plat_info.c (platform.md 3.11, command-mode.md 5.6): the text made of
 * plat_get_info()'s answer, its log lines and the named command 'info'
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include <pb/command.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/plat_info.h>

/* What the fake platform answers */
static struct {
    int rc;
    struct pb_plat_info info;
    const char *extra;
    size_t extra_size; /* The room plat_info.c gave it */
} fake;

int plat_get_info(struct pb_plat_info *info, char *extra, size_t extra_size)
{
    fake.extra_size = extra_size;
    if (fake.rc != PB_OK)
        return fake.rc;

    *info = fake.info;
    (void)snprintf(extra, extra_size, "%s", fake.extra);
    return PB_OK;
}

static int setup(void **state)
{
    (void)state;
    memset(&fake, 0, sizeof(fake));
    fake.info.soc = "i.MX6UL";
    fake.info.rev = "1.2";
    fake.info.cpu_hz = 528000000;
    fake.info.mem_hz = 396000000;
    fake.extra = "Speed grade 1\n";
    return 0;
}

static const struct cm_command *info_command(void)
{
    for (const struct cm_command *c = __start_pb_commands; c < __stop_pb_commands; c++) {
        if (strcmp(c->name, "info") == 0)
            return c;
    }

    fail_msg("No command 'info'");
    return NULL;
}

/* The first line and the specifics, the rates in MHz */
static void test_text(void **state)
{
    char out[256];
    const char expected[] = "SoC i.MX6UL rev 1.2, CPU 528 MHz, memory 396 MHz\nSpeed grade 1\n";
    (void)state;

    assert_int_equal(plat_info_text(out, sizeof(out)), (int)strlen(expected));
    assert_string_equal(out, expected);
    assert_int_equal(fake.extra_size, 512);
}

/* platform.md 3.11: 0 and NULL say what isn't known */
static void test_unknown(void **state)
{
    char out[256];
    (void)state;

    fake.info.soc = NULL;
    fake.info.rev = NULL;
    fake.info.cpu_hz = 0;
    fake.info.mem_hz = 0;
    fake.extra = "";
    assert_true(plat_info_text(out, sizeof(out)) > 0);
    assert_string_equal(out, "SoC unknown rev unknown, CPU unknown, memory none set up\n");
}

/* A buffer too small for the text gets what fits, and the length that fits */
static void test_truncated(void **state)
{
    char out[16];
    (void)state;

    assert_int_equal(plat_info_text(out, sizeof(out)), (int)sizeof(out) - 1);
    assert_string_equal(out, "SoC i.MX6UL rev");
    assert_int_equal(plat_info_text(out, 0), -PB_ERR_BUF_TOO_SMALL);
}

/* The platform's error is the text's, and the text is empty */
static void test_platform_error(void **state)
{
    char out[64];
    (void)state;

    memset(out, 'x', sizeof(out));
    fake.rc = -PB_ERR_NOT_SUPPORTED;
    assert_int_equal(plat_info_text(out, sizeof(out)), -PB_ERR_NOT_SUPPORTED);
    assert_string_equal(out, "");

    /* The log says so, and goes on */
    plat_info_log();
}

/* A log line each: also a last line without its newline */
static void test_log(void **state)
{
    (void)state;

    fake.extra = "Speed grade 1\nBoot config: fuses";
    plat_info_log();
    fake.extra = "";
    plat_info_log();
}

/* command-mode.md 5.6: 'info' answers the text and its NUL, in the room it
 * has; no room is an error with nothing written */
static void test_command(void **state)
{
    const struct cm_command *c = info_command();
    uint8_t response[256];
    size_t size = sizeof(response);
    const char expected[] = "SoC i.MX6UL rev 1.2, CPU 528 MHz, memory 396 MHz\nSpeed grade 1\n";
    (void)state;

    assert_int_equal(c->run(NULL, 0, response, &size), PB_OK);
    assert_int_equal(size, sizeof(expected));
    assert_memory_equal(response, expected, sizeof(expected));

    size = 20;
    assert_int_equal(c->run(NULL, 0, response, &size), PB_OK);
    assert_int_equal(size, 20);
    assert_int_equal(response[19], '\0');

    size = 0;
    assert_int_equal(c->run(NULL, 0, response, &size), -PB_ERR_BUF_TOO_SMALL);
    assert_int_equal(size, 0);

    fake.rc = -PB_ERR_IO;
    size = sizeof(response);
    assert_int_equal(c->run(NULL, 0, response, &size), -PB_ERR_IO);
    assert_int_equal(size, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_text, setup),
        cmocka_unit_test_setup(test_unknown, setup),
        cmocka_unit_test_setup(test_truncated, setup),
        cmocka_unit_test_setup(test_platform_error, setup),
        cmocka_unit_test_setup(test_log, setup),
        cmocka_unit_test_setup(test_command, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
