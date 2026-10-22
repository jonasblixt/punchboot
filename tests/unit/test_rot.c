/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Root of trust (src/rot.c) and security life cycle (src/slc.c), with the
 * internal keystore and fake board callbacks. Both modules keep their config
 * in a static, so the "not initialized" tests run first.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <string.h>

/* The keys in pki/internal_keystore.bpak */
#define KEY_P256 0xa90f9680
#define KEY_P384 0x25c6dd36
#define KEY_P521 0x52c1eda0

static uint32_t revoked; /* Bit per key, by param1 */
static int revoke_calls;

static int fake_revoke_key(const struct rot_key *key)
{
    revoke_calls++;
    revoked |= 1u << key->param1;
    return PB_OK;
}

static int fake_read_key_status(const struct rot_key *key)
{
    return (revoked & (1u << key->param1)) ? -PB_ERR_KEY_REVOKED : PB_OK;
}

static const struct rot_config rot_cfg = {
    .revoke_key = fake_revoke_key,
    .read_key_status = fake_read_key_status,
    .key_map_length = 3,
    .key_map = {
        { .name = "p256", .id = KEY_P256, .param1 = 0 },
        { .name = "p384", .id = KEY_P384, .param1 = 1 },
        { .name = "p521", .id = KEY_P521, .param1 = 2 },
    },
};

static const struct rot_config rot_cfg_no_callbacks = {
    .key_map_length = 3,
    .key_map = {
        { .name = "p256", .id = KEY_P256 },
        { .name = "p384", .id = KEY_P384 },
        { .name = "p521", .id = KEY_P521 },
    },
};

static const struct rot_config rot_cfg_too_few_keys = {
    .revoke_key = fake_revoke_key,
    .read_key_status = fake_read_key_status,
    .key_map_length = 2,
    .key_map = {
        { .name = "p256", .id = KEY_P256 },
        { .name = "p384", .id = KEY_P384 },
    },
};

static const struct rot_config rot_cfg_wrong_order = {
    .revoke_key = fake_revoke_key,
    .read_key_status = fake_read_key_status,
    .key_map_length = 3,
    .key_map = {
        { .name = "p384", .id = KEY_P384 },
        { .name = "p256", .id = KEY_P256 },
        { .name = "p521", .id = KEY_P521 },
    },
};

static void test_rot_not_initialized(void **state)
{
    dsa_t kind;
    const uint8_t *der;
    size_t der_length;
    (void)state;

    assert_int_equal(rot_read_key_status(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal((int)rot_no_of_keys(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_read_key_status_by_idx(0), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal((int)rot_key_idx_to_id(0), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_revoke_key(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_get_dsa_key(KEY_P256, &kind, &der, &der_length), -PB_ERR_NOT_SUPPORTED);
}

static void test_rot_init_checks_the_keystore(void **state)
{
    (void)state;

    assert_int_equal(rot_init(&rot_cfg_too_few_keys), -PB_ERR_PARAM);
    assert_int_equal(rot_init(&rot_cfg_wrong_order), -PB_ERR_PARAM);
    /* Still not initialized */
    assert_int_equal(rot_revoke_key(KEY_P256), -PB_ERR_NOT_SUPPORTED);
}

static void test_rot_without_callbacks(void **state)
{
    dsa_t kind;
    const uint8_t *der;
    size_t der_length;
    (void)state;

    assert_int_equal(rot_init(&rot_cfg_no_callbacks), PB_OK);
    assert_int_equal(rot_no_of_keys(), 3);
    assert_int_equal(rot_key_idx_to_id(1), KEY_P384);
    assert_int_equal(rot_read_key_status(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_read_key_status_by_idx(0), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_revoke_key(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_get_dsa_key(KEY_P256, &kind, &der, &der_length), -PB_ERR_NOT_SUPPORTED);
}

static void test_rot_key_status(void **state)
{
    (void)state;

    revoked = 0;
    assert_int_equal(rot_init(&rot_cfg), PB_OK);
    assert_int_equal(rot_no_of_keys(), 3);

    assert_int_equal(rot_read_key_status(KEY_P384), PB_OK);
    assert_int_equal(rot_read_key_status(0x12345678), -PB_ERR_KEY_NOT_FOUND);

    assert_int_equal(rot_read_key_status_by_idx(2), PB_OK);
    assert_int_equal(rot_read_key_status_by_idx(3), -PB_ERR_PARAM);
    assert_int_equal(rot_key_idx_to_id(2), KEY_P521);
    assert_int_equal((int)rot_key_idx_to_id(3), -PB_ERR_PARAM);
}

static void test_rot_get_dsa_key(void **state)
{
    static const struct {
        key_id_t id;
        dsa_t kind;
    } keys[] = {
        { KEY_P256, DSA_EC_SECP256r1 },
        { KEY_P384, DSA_EC_SECP384r1 },
        { KEY_P521, DSA_EC_SECP521r1 },
    };

    dsa_t kind;
    const uint8_t *der;
    size_t der_length;
    (void)state;

    revoked = 0;
    for (size_t i = 0; i < ARRAY_SIZE(keys); i++) {
        der = NULL;
        der_length = 0;
        assert_int_equal(rot_get_dsa_key(keys[i].id, &kind, &der, &der_length), PB_OK);
        assert_int_equal(kind, keys[i].kind);
        assert_non_null(der);
        /* A DER SubjectPublicKeyInfo */
        assert_int_equal(der[0], 0x30);
        assert_true(der_length > 64);
    }

    assert_int_equal(rot_get_dsa_key(0x12345678, &kind, &der, &der_length), -PB_ERR_KEY_NOT_FOUND);
}

static void test_rot_revoke(void **state)
{
    dsa_t kind;
    const uint8_t *der;
    size_t der_length;
    (void)state;

    revoked = 0;
    revoke_calls = 0;

    assert_int_equal(rot_revoke_key(0x12345678), -PB_ERR_KEY_NOT_FOUND);
    assert_int_equal(revoke_calls, 0);

    assert_int_equal(rot_revoke_key(KEY_P384), PB_OK);
    assert_int_equal(revoke_calls, 1);
    assert_int_equal(rot_read_key_status(KEY_P384), -PB_ERR_KEY_REVOKED);
    assert_int_equal(rot_read_key_status_by_idx(1), -PB_ERR_KEY_REVOKED);
    assert_int_equal(rot_get_dsa_key(KEY_P384, &kind, &der, &der_length), -PB_ERR_KEY_REVOKED);
    /* The others are still good */
    assert_int_equal(rot_read_key_status(KEY_P256), PB_OK);
}

static slc_t slc_state;

static slc_t fake_read_status(void)
{
    return slc_state;
}

static int fake_set_configuration(void)
{
    slc_state = SLC_CONFIGURATION;
    return PB_OK;
}

static int fake_set_configuration_locked(void)
{
    slc_state = SLC_CONFIGURATION_LOCKED;
    return PB_OK;
}

static int fake_set_eol(void)
{
    slc_state = SLC_EOL;
    return PB_OK;
}

static void test_slc_not_initialized(void **state)
{
    (void)state;

    assert_int_equal(slc_read_status(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_configuration(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
}

static void test_slc_without_callbacks(void **state)
{
    static const struct slc_config cfg = { 0 };
    (void)state;

    /* A board without set_configuration_locked, but with set_eol, would
     * crash in slc_set_eol() (F-001), so that is not tested */
    assert_int_equal(slc_init(&cfg), PB_OK);
    assert_int_equal(slc_read_status(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_configuration(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
}

static void test_slc_life_cycle(void **state)
{
    static const struct slc_config cfg = {
        .read_status = fake_read_status,
        .set_configuration = fake_set_configuration,
        .set_configuration_locked = fake_set_configuration_locked,
        .set_eol = fake_set_eol,
    };
    (void)state;

    slc_state = SLC_NOT_CONFIGURED;
    assert_int_equal(slc_init(&cfg), PB_OK);
    assert_int_equal(slc_read_status(), SLC_NOT_CONFIGURED);
    assert_int_equal(slc_set_configuration(), PB_OK);
    assert_int_equal(slc_read_status(), SLC_CONFIGURATION);
    assert_int_equal(slc_set_configuration_locked(), PB_OK);
    assert_int_equal(slc_read_status(), SLC_CONFIGURATION_LOCKED);
    assert_int_equal(slc_set_eol(), PB_OK);
    assert_int_equal(slc_read_status(), SLC_EOL);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rot_not_initialized),
        cmocka_unit_test(test_rot_init_checks_the_keystore),
        cmocka_unit_test(test_rot_without_callbacks),
        cmocka_unit_test(test_rot_key_status),
        cmocka_unit_test(test_rot_get_dsa_key),
        cmocka_unit_test(test_rot_revoke),
        cmocka_unit_test(test_slc_not_initialized),
        cmocka_unit_test(test_slc_without_callbacks),
        cmocka_unit_test(test_slc_life_cycle),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
