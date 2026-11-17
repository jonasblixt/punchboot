/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The keys' revoke slots (board.md 7): key n is in slot n, and a dropped key
 * leaves its slot empty. The keystore is the test's, as gen-keystore.py writes
 * it for VERIFY_KEY0, VERIFY_KEY2 and VERIFY_KEY3: slot 1's key was dropped.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/keys.h>
#include <pb/pb.h>
#include <pb/rot.h>

#define KEY_A 0xa90f9680
#define KEY_B 0x25c6dd36
#define KEY_C 0x52c1eda0

const struct pb_key pb_keys[] = {
    { .id = KEY_A, .kind = DSA_EC_SECP256r1, .slot = 0 },
    { .id = KEY_B, .kind = DSA_EC_SECP384r1, .slot = 2 },
    { .id = KEY_C, .kind = DSA_EC_SECP521r1, .slot = 3 },
};
const unsigned int pb_no_of_keys = 3;

static uint32_t revoked; /* Bit per slot */

static int fake_revoke_key(const struct rot_key *key)
{
    revoked |= 1u << key->slot;
    return PB_OK;
}

static int fake_read_key_status(const struct rot_key *key)
{
    return (revoked & (1u << key->slot)) ? -PB_ERR_KEY_REVOKED : PB_OK;
}

static const struct rot_config rot_cfg = {
    .revoke_key = fake_revoke_key,
    .read_key_status = fake_read_key_status,
};

static int setup(void **state)
{
    (void)state;
    revoked = 0;
    assert_int_equal(rot_init(&rot_cfg), PB_OK);
    return 0;
}

/* Each key revokes its own slot, the keystore's index is not the slot */
static void test_slot_is_the_keys(void **state)
{
    (void)state;

    assert_int_equal(rot_revoke_key(KEY_A), PB_OK);
    assert_int_equal(revoked, 1u << 0);
    assert_int_equal(rot_revoke_key(KEY_B), PB_OK);
    assert_int_equal(revoked, (1u << 0) | (1u << 2));
    assert_int_equal(rot_read_key_status(KEY_A), -PB_ERR_KEY_REVOKED);
    assert_int_equal(rot_read_key_status(KEY_B), -PB_ERR_KEY_REVOKED);
    assert_int_equal(rot_read_key_status(KEY_C), PB_OK);
    assert_int_equal(rot_read_key_status_by_idx(2), PB_OK);
}

/* Slot 1, the dropped key's, is burnt: no key reads it */
static void test_dropped_key(void **state)
{
    (void)state;

    revoked = 1u << 1;
    assert_int_equal(rot_read_key_status(KEY_A), PB_OK);
    assert_int_equal(rot_read_key_status(KEY_B), PB_OK);
    assert_int_equal(rot_read_key_status(KEY_C), PB_OK);
    for (unsigned int i = 0; i < pb_no_of_keys; i++)
        assert_int_equal(rot_read_key_status_by_idx(i), PB_OK);
}

static void test_unknown_key(void **state)
{
    (void)state;

    assert_int_equal(rot_revoke_key(0xdeadbeef), -PB_ERR_KEY_NOT_FOUND);
    assert_int_equal(rot_read_key_status(0xdeadbeef), -PB_ERR_KEY_NOT_FOUND);
    assert_int_equal(revoked, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_slot_is_the_keys, setup),
        cmocka_unit_test_setup(test_dropped_key, setup),
        cmocka_unit_test_setup(test_unknown_key, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
