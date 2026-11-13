/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * libpbstate (tools/pbstate/src/pbstate.c), the OS's side of the A/B state
 * (boot.md 3.5, 3.7), on two files in a temporary directory that stand for
 * the primary and the backup state partitions.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cmocka.h>

#include <boot/pb_state_blob.h>
#include <pb/crc.h>

#include "pbstate.h"

static char dir[] = "/tmp/test_pbstate.XXXXXX";
static char primary[sizeof(dir) + 16];
static char backup[sizeof(dir) + 16];

static void write_file(const char *path, const struct pb_boot_state *s)
{
    FILE *f = fopen(path, "wb");

    assert_non_null(f);
    assert_int_equal(fwrite(s, sizeof(*s), 1, f), 1);
    assert_int_equal(fclose(f), 0);
}

static void read_file(const char *path, struct pb_boot_state *s)
{
    FILE *f = fopen(path, "rb");

    assert_non_null(f);
    assert_int_equal(fread(s, sizeof(*s), 1, f), 1);
    assert_int_equal(fclose(f), 0);
}

/* A valid state in both copies, as pb writes it */
static void write_state(uint32_t enable, uint32_t verified, uint32_t attempts, uint32_t error)
{
    struct pb_boot_state s;

    memset(&s, 0, sizeof(s));
    s.magic = PB_STATE_MAGIC;
    s.enable = enable;
    s.verified = verified;
    s.remaining_boot_attempts = attempts;
    s.error = error;
    s.crc = crc32(0, (const uint8_t *)&s, sizeof(s));
    write_file(primary, &s);
    write_file(backup, &s);
}

/* Both copies are valid, the same, and hold these values */
static void assert_state(uint32_t enable, uint32_t verified, uint32_t attempts, uint32_t error)
{
    struct pb_boot_state p, b;
    uint32_t crc;

    read_file(primary, &p);
    read_file(backup, &b);
    assert_memory_equal(&p, &b, sizeof(p));

    crc = p.crc;
    p.crc = 0;
    assert_int_equal(crc, crc32(0, (const uint8_t *)&p, sizeof(p)));
    assert_int_equal(p.magic, PB_STATE_MAGIC);
    assert_int_equal(p.enable, enable);
    assert_int_equal(p.verified, verified);
    assert_int_equal(p.remaining_boot_attempts, attempts);
    assert_int_equal(p.error, error);
}

static int group_setup(void **state)
{
    (void)state;

    if (mkdtemp(dir) == NULL)
        return -1;

    snprintf(primary, sizeof(primary), "%s/primary", dir);
    snprintf(backup, sizeof(backup), "%s/backup", dir);
    return pbstate_init(primary, backup, NULL);
}

static int group_teardown(void **state)
{
    (void)state;
    unlink(primary);
    unlink(backup);
    return rmdir(dir);
}

/* boot.md 3.5, rule 1: the OS confirms the enabled slot's trial, which ends
 * it */
static void test_confirming_the_enabled_slot_ends_its_trial(void **state)
{
    (void)state;

    write_state(PB_STATE_A_ENABLED, 0, 3, 0);
    assert_int_equal(pbstate_set_system_verified(PBSTATE_SYSTEM_A), 0);
    assert_state(PB_STATE_A_ENABLED, PB_STATE_A_VERIFIED, 0, 0);

    write_state(PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
    assert_int_equal(pbstate_set_system_verified(PBSTATE_SYSTEM_B), 0);
    assert_state(PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED | PB_STATE_B_VERIFIED, 0, 0);
}

/*
 * boot.md 3.5, rule 3: a trial that failed is tried again on the next boots
 * until its attempts run out. Confirming the other slot, which the fallback
 * boot runs, marks that slot and leaves the trial going (F-180: it ended the
 * trial, and the next boot rolled back)
 */
static void test_confirming_the_other_slot_keeps_the_trial(void **state)
{
    (void)state;

    write_state(PB_STATE_A_ENABLED, 0, 3, 0);
    assert_int_equal(pbstate_set_system_verified(PBSTATE_SYSTEM_B), 0);
    assert_state(PB_STATE_A_ENABLED, PB_STATE_B_VERIFIED, 3, 0);
    assert_int_equal(pbstate_is_system_active(PBSTATE_SYSTEM_A), 1);
    assert_int_equal(pbstate_is_system_verified(PBSTATE_SYSTEM_A), 0);

    write_state(PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
    assert_int_equal(pbstate_set_system_verified(PBSTATE_SYSTEM_A), 0);
    assert_state(PB_STATE_B_ENABLED, PB_STATE_A_VERIFIED, 2, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_confirming_the_enabled_slot_ends_its_trial),
        cmocka_unit_test(test_confirming_the_other_slot_keeps_the_trial),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
