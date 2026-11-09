/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The keys' revoke slots, CONFIG_KEYSTORE_SLOTS. src/rot.c is included to
 * set its slot list per test.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include "../../src/rot.c"

/* The keys in pki/internal_keystore.bpak, in its order */
#define KEY_P256 0xa90f9680
#define KEY_P384 0x25c6dd36
#define KEY_P521 0x52c1eda0

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
    return 0;
}

/* Empty: the keystore's order */
static void test_keystore_order(void **state)
{
    (void)state;

    slots = "";
    assert_int_equal(rot_init(&rot_cfg), PB_OK);
    assert_int_equal(rot_revoke_key(KEY_P384), PB_OK);
    assert_int_equal(revoked, 1u << 1);
}

/* A dropped key keeps its slot, the keys after it keep theirs */
static void test_dropped_key(void **state)
{
    (void)state;

    slots = "52c1eda0 deadbeef a90f9680 25c6dd36";
    assert_int_equal(rot_init(&rot_cfg), PB_OK);
    assert_int_equal(rot_revoke_key(KEY_P256), PB_OK);
    assert_int_equal(revoked, 1u << 2);
    assert_int_equal(rot_read_key_status(KEY_P256), -PB_ERR_KEY_REVOKED);
    assert_int_equal(rot_read_key_status(KEY_P384), PB_OK);
    assert_int_equal(rot_read_key_status(KEY_P521), PB_OK);

    /* Slot 1, the dropped key's, is burnt: no key reads it */
    revoked = 1u << 1;
    assert_int_equal(rot_read_key_status(KEY_P256), PB_OK);
    assert_int_equal(rot_read_key_status(KEY_P384), PB_OK);
    assert_int_equal(rot_read_key_status(KEY_P521), PB_OK);
    assert_int_equal(rot_read_key_status_by_idx(0), PB_OK);
}

static void test_spaces_and_prefix(void **state)
{
    (void)state;

    slots = "  0x25c6dd36   0xA90F9680 52c1eda0 ";
    assert_int_equal(rot_init(&rot_cfg), PB_OK);
    assert_int_equal(rot_revoke_key(KEY_P256), PB_OK);
    assert_int_equal(revoked, 1u << 1);
}

/* rot_init() refuses a list it can't read, or one without a slot for a key */
static void test_bad_lists(void **state)
{
    static const char *bad[] = {
        "a90f9680 zz 25c6dd36 52c1eda0",
        "a90f9680 25c6dd36 52c1eda0 zz",
        "a90f9680,25c6dd36,52c1eda0",
        "a90f9680x 25c6dd36 52c1eda0",
        "a90f9680 52c1eda0",
        "deadbeef",
    };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(bad); i++) {
        slots = bad[i];
        assert_int_equal(rot_init(&rot_cfg), -PB_ERR_PARAM);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_keystore_order, setup),
        cmocka_unit_test_setup(test_dropped_key, setup),
        cmocka_unit_test_setup(test_spaces_and_prefix, setup),
        cmocka_unit_test_setup(test_bad_lists, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
