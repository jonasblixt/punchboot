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

static uint32_t revoked; /* Bit per key, by index */
static int revoke_calls;

static int fake_revoke_key(const struct rot_key *key)
{
    revoke_calls++;
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

static const struct rot_config rot_cfg_no_callbacks = { 0 };

static void test_rot_not_initialized(void **state)
{
    dsa_t kind;
    const uint8_t *der;
    size_t der_length;
    size_t count = 77;
    key_id_t id = 0x77;
    (void)state;

    assert_int_equal(rot_read_key_status(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    /* An error is the result, and no count or id comes with it (F-346): the
     * error was the count, 2^64 - 19 keys, and the id */
    assert_int_equal(rot_no_of_keys(&count), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(count, 77);
    assert_int_equal(rot_read_key_status_by_idx(0), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_key_idx_to_id(0, &id), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(id, 0x77);
    assert_int_equal(rot_revoke_key(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_get_dsa_key(KEY_P256, &kind, &der, &der_length), -PB_ERR_NOT_SUPPORTED);
}

static void test_rot_init_needs_a_config(void **state)
{
    (void)state;

    assert_int_equal(rot_init(NULL), -PB_ERR_PARAM);
    /* Still not initialized */
    assert_int_equal(rot_revoke_key(KEY_P256), -PB_ERR_NOT_SUPPORTED);
}

static void test_rot_without_callbacks(void **state)
{
    dsa_t kind;
    const uint8_t *der;
    size_t der_length;
    size_t count = 0;
    key_id_t id = 0;
    (void)state;

    assert_int_equal(rot_init(&rot_cfg_no_callbacks), PB_OK);
    assert_int_equal(rot_no_of_keys(&count), PB_OK);
    assert_int_equal(count, 3);
    assert_int_equal(rot_key_idx_to_id(1, &id), PB_OK);
    assert_int_equal(id, KEY_P384);
    assert_int_equal(rot_read_key_status(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_read_key_status_by_idx(0), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_revoke_key(KEY_P256), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(rot_get_dsa_key(KEY_P256, &kind, &der, &der_length), -PB_ERR_NOT_SUPPORTED);
}

static void test_rot_key_status(void **state)
{
    size_t count = 0;
    key_id_t id = 0;
    (void)state;

    revoked = 0;
    assert_int_equal(rot_init(&rot_cfg), PB_OK);
    assert_int_equal(rot_no_of_keys(&count), PB_OK);
    assert_int_equal(count, 3);

    assert_int_equal(rot_read_key_status(KEY_P384), PB_OK);
    assert_int_equal(rot_read_key_status(0x12345678), -PB_ERR_KEY_NOT_FOUND);

    assert_int_equal(rot_read_key_status_by_idx(2), PB_OK);
    assert_int_equal(rot_read_key_status_by_idx(3), -PB_ERR_PARAM);
    assert_int_equal(rot_key_idx_to_id(2, &id), PB_OK);
    assert_int_equal(id, KEY_P521);
    /* An index the keystore doesn't have: no id, the one before stays */
    assert_int_equal(rot_key_idx_to_id(3, &id), -PB_ERR_PARAM);
    assert_int_equal(id, KEY_P521);
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
static int slc_writes;

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
    slc_writes++;
    slc_state = SLC_EOL;
    return PB_OK;
}

static void test_slc_not_initialized(void **state)
{
    (void)state;

    /* Unknown, not unsupported: command mode treats it as locked */
    assert_int_equal(slc_read_status(), -PB_ERR_STATE);
    assert_int_equal(slc_set_configuration(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
}

static void test_slc_without_callbacks(void **state)
{
    static const struct slc_config cfg = { 0 };
    (void)state;

    assert_int_equal(slc_init(&cfg), PB_OK);
    assert_int_equal(slc_read_status(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_configuration(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
}

/* A board that can lock but has no EOL, like jiffy (F-001) */
static void test_slc_without_eol(void **state)
{
    static const struct slc_config cfg = {
        .read_status = fake_read_status,
        .set_configuration_locked = fake_set_configuration_locked,
    };
    (void)state;

    slc_state = SLC_CONFIGURATION;
    assert_int_equal(slc_init(&cfg), PB_OK);
    assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(slc_read_status(), SLC_CONFIGURATION);
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

/* Forward only, and setting the current state again writes nothing */
static void test_slc_order(void **state)
{
    static const struct slc_config cfg = {
        .read_status = fake_read_status,
        .set_configuration = fake_set_configuration,
        .set_configuration_locked = fake_set_configuration_locked,
        .set_eol = fake_set_eol,
    };
    (void)state;

    assert_int_equal(slc_init(&cfg), PB_OK);

    /* Not configured: no lock */
    slc_state = SLC_NOT_CONFIGURED;
    slc_writes = 0;
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_STATE);
    assert_int_equal(slc_writes, 0);

    /* The configuration can be written again */
    slc_state = SLC_CONFIGURATION;
    assert_int_equal(slc_set_configuration(), PB_OK);
    assert_int_equal(slc_writes, 1);

    /* Locked: no configuration, locking again is a no-op */
    slc_state = SLC_CONFIGURATION_LOCKED;
    slc_writes = 0;
    assert_int_equal(slc_set_configuration(), -PB_ERR_STATE);
    assert_int_equal(slc_set_configuration_locked(), PB_OK);
    assert_int_equal(slc_writes, 0);

    /* EOL from any state, once */
    slc_state = SLC_CONFIGURATION;
    assert_int_equal(slc_set_eol(), PB_OK);
    assert_int_equal(slc_writes, 1);
    assert_int_equal(slc_set_eol(), PB_OK);
    assert_int_equal(slc_set_configuration(), -PB_ERR_STATE);
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_STATE);
    assert_int_equal(slc_writes, 1);

    /* A failed read writes nothing */
    slc_state = -PB_ERR_IO;
    slc_writes = 0;
    assert_int_equal(slc_set_configuration(), -PB_ERR_IO);
    assert_int_equal(slc_set_configuration_locked(), -PB_ERR_IO);
    assert_int_equal(slc_set_eol(), -PB_ERR_IO);
    assert_int_equal(slc_writes, 0);
}

/* The board's configuration fuses, by index */
static uint32_t fuses[8];
static int fuse_writes;
static int fuse_rc;

static int fake_fuse_read(uint32_t index, uint32_t *value)
{
    if (fuse_rc != PB_OK)
        return fuse_rc;

    *value = fuses[index];
    return PB_OK;
}

static int fake_fuse_write(uint32_t index, uint32_t value)
{
    fuse_writes++;
    fuses[index] |= value;
    return PB_OK;
}

static const struct slc_fuse boot_fuses[] = {
    { 5, 0x0000c060 },
    { 6, 0x00000010 },
};

static void test_slc_fuses(void **state)
{
    static const struct slc_config cfg = {
        .read_status = fake_read_status,
        .set_configuration = fake_set_configuration,
        .fuses = boot_fuses,
        .fuses_length = ARRAY_SIZE(boot_fuses),
        .fuse_read = fake_fuse_read,
        .fuse_write = fake_fuse_write,
    };
    (void)state;

    slc_state = SLC_NOT_CONFIGURED;
    slc_writes = 0;
    assert_int_equal(slc_init(&cfg), PB_OK);
    assert_int_equal(slc_set_configuration(), PB_OK);
    assert_int_equal(slc_writes, 1);
    assert_int_equal(fuse_writes, 2);
    assert_int_equal(fuses[5], 0x0000c060);
    assert_int_equal(fuses[6], 0x00000010);

    /* Set already, also with other bits in the word (e.g. the closed bit) */
    fuse_writes = 0;
    fuses[6] |= 0x02;
    assert_int_equal(slc_set_configuration(), PB_OK);
    assert_int_equal(fuse_writes, 0);

    /* A word that has some of the bits gets the rest */
    fuses[5] = 0x00000060;
    assert_int_equal(slc_set_configuration(), PB_OK);
    assert_int_equal(fuse_writes, 1);
    assert_int_equal(fuses[5], 0x0000c060);

    /* A read error stops it */
    fuse_writes = 0;
    memset(fuses, 0, sizeof(fuses));
    fuse_rc = -PB_ERR_IO;
    assert_int_equal(slc_set_configuration(), -PB_ERR_IO);
    assert_int_equal(fuse_writes, 0);
    fuse_rc = PB_OK;

    /* Locked: nothing is written */
    slc_state = SLC_CONFIGURATION_LOCKED;
    assert_int_equal(slc_set_configuration(), -PB_ERR_STATE);
    assert_int_equal(fuse_writes, 0);
}

static int failing_set_configuration(void)
{
    return -PB_ERR_IO;
}

/* The fuses alone are a configuration, they need a read and a write */
static void test_slc_fuses_config(void **state)
{
    static const struct slc_config fuses_only = {
        .read_status = fake_read_status,
        .fuses = boot_fuses,
        .fuses_length = ARRAY_SIZE(boot_fuses),
        .fuse_read = fake_fuse_read,
        .fuse_write = fake_fuse_write,
    };
    static const struct slc_config no_write = {
        .read_status = fake_read_status,
        .fuses = boot_fuses,
        .fuses_length = ARRAY_SIZE(boot_fuses),
        .fuse_read = fake_fuse_read,
    };
    static const struct slc_config failing = {
        .read_status = fake_read_status,
        .set_configuration = failing_set_configuration,
        .fuses = boot_fuses,
        .fuses_length = ARRAY_SIZE(boot_fuses),
        .fuse_read = fake_fuse_read,
        .fuse_write = fake_fuse_write,
    };
    (void)state;

    memset(fuses, 0, sizeof(fuses));
    fuse_writes = 0;
    slc_state = SLC_NOT_CONFIGURED;
    assert_int_equal(slc_init(&fuses_only), PB_OK);
    assert_int_equal(slc_set_configuration(), PB_OK);
    assert_int_equal(fuse_writes, 2);

    assert_int_equal(slc_init(&no_write), -PB_ERR_PARAM);

    /* The platform's part first: when it fails, no fuse is written */
    memset(fuses, 0, sizeof(fuses));
    fuse_writes = 0;
    assert_int_equal(slc_init(&failing), PB_OK);
    assert_int_equal(slc_set_configuration(), -PB_ERR_IO);
    assert_int_equal(fuse_writes, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rot_not_initialized),
        cmocka_unit_test(test_rot_init_needs_a_config),
        cmocka_unit_test(test_rot_without_callbacks),
        cmocka_unit_test(test_rot_key_status),
        cmocka_unit_test(test_rot_get_dsa_key),
        cmocka_unit_test(test_rot_revoke),
        cmocka_unit_test(test_slc_not_initialized),
        cmocka_unit_test(test_slc_without_callbacks),
        cmocka_unit_test(test_slc_without_eol),
        cmocka_unit_test(test_slc_life_cycle),
        cmocka_unit_test(test_slc_fuses),
        cmocka_unit_test(test_slc_fuses_config),
        cmocka_unit_test(test_slc_order),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
