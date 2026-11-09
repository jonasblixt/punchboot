/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HABv4 life cycle and SRK revocation (src/drivers/fuse/imx_hab_fuses.c)
 * on a fake OCOTP with the 6UL's layout: 8 banks of 8 words.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/fuse/imx_hab_fuses.h>
#include <drivers/fuse/imx_ocotp.h>
#include <pb/pb.h>
#include <string.h>

#define WORDS_PER_BANK 8

static uint32_t otp[8 * WORDS_PER_BANK];
static int writes;
static int read_rc;
static int revoke_allowed_rc;

int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value)
{
    if (read_rc != PB_OK)
        return read_rc;

    *value = otp[bank * WORDS_PER_BANK + row];
    return PB_OK;
}

/* An OTP write only blows bits */
int imx_ocotp_write(uint32_t bank, uint32_t row, uint32_t value)
{
    writes++;
    otp[bank * WORDS_PER_BANK + row] |= value;
    return PB_OK;
}

static int revoke_allowed(void)
{
    return revoke_allowed_rc;
}

static const struct imx_hab_fuses fuses = {
    .sec_config_bank = 0,
    .sec_config_word = 6,
    .closed = 0x02,
    .srk_bank = 3,
    .srk_word = 0,
    .revoke_bank = 5,
    .revoke_word = 7,
    .revocable = 3,
    .revoke_allowed = revoke_allowed,
};

static const uint32_t srk[IMX_HAB_SRK_HASH_WORDS] = {
    0x5020c7d7, 0xbb62b945, 0xdd97c8be, 0xdc6710dd, 0x2756b777, 0xef43bc0a, 0x7185604b, 0x3f335991,
};

#define SEC_CONFIG (otp[0 * WORDS_PER_BANK + 6])
#define SRK(n)     (otp[3 * WORDS_PER_BANK + (n)])
#define REVOKE     (otp[5 * WORDS_PER_BANK + 7])

static int setup(void **state)
{
    (void)state;
    memset(otp, 0, sizeof(otp));
    writes = 0;
    read_rc = PB_OK;
    revoke_allowed_rc = PB_OK;
    imx_hab_fuses_init(&fuses);
    return 0;
}

static void test_not_initialized(void **state)
{
    struct rot_key key = { .id = 1, .slot = 0 };
    (void)state;

    imx_hab_fuses_init(NULL);
    assert_int_equal(imx_hab_slc_read_status(), -PB_ERR_STATE);
    assert_int_equal(imx_hab_slc_set_configuration_locked(), -PB_ERR_STATE);
    assert_int_equal(imx_hab_read_key_status(&key), -PB_ERR_STATE);
    assert_int_equal(imx_hab_revoke_key(&key), -PB_ERR_STATE);
    assert_int_equal(imx_hab_fuse_srk(srk, IMX_HAB_SRK_HASH_WORDS), -PB_ERR_STATE);
    assert_int_equal(writes, 0);
}

static void test_life_cycle(void **state)
{
    (void)state;

    assert_int_equal(imx_hab_slc_read_status(), SLC_NOT_CONFIGURED);

    /* One word is enough: fusing it was started */
    SRK(7) = srk[7];
    assert_int_equal(imx_hab_slc_read_status(), SLC_CONFIGURATION);

    assert_int_equal(imx_hab_fuse_srk(srk, IMX_HAB_SRK_HASH_WORDS), PB_OK);

    for (unsigned int n = 0; n < IMX_HAB_SRK_HASH_WORDS; n++)
        assert_int_equal(SRK(n), srk[n]);

    assert_int_equal(imx_hab_slc_set_configuration_locked(), PB_OK);
    assert_int_equal(SEC_CONFIG, 0x02);
    assert_int_equal(imx_hab_slc_read_status(), SLC_CONFIGURATION_LOCKED);

    /* A failed read is an error, not a state */
    read_rc = -PB_ERR_IO;
    assert_int_equal(imx_hab_slc_read_status(), -PB_ERR_IO);
}

static void test_fuse_srk(void **state)
{
    uint32_t other[IMX_HAB_SRK_HASH_WORDS];
    (void)state;

    assert_int_equal(imx_hab_fuse_srk(srk, IMX_HAB_SRK_HASH_WORDS - 1), -PB_ERR_PARAM);

    /* An empty word can't be right */
    memcpy(other, srk, sizeof(other));
    other[3] = 0;
    assert_int_equal(imx_hab_fuse_srk(other, IMX_HAB_SRK_HASH_WORDS), -PB_ERR_PARAM);
    assert_int_equal(writes, 0);

    /* Fused already: nothing to write */
    for (unsigned int n = 0; n < IMX_HAB_SRK_HASH_WORDS; n++)
        SRK(n) = srk[n];

    assert_int_equal(imx_hab_fuse_srk(srk, IMX_HAB_SRK_HASH_WORDS), PB_OK);
    assert_int_equal(writes, 0);

    /* Another hash */
    other[3] = 0x12345678;
    assert_int_equal(imx_hab_fuse_srk(other, IMX_HAB_SRK_HASH_WORDS), -PB_ERR_MEM);
    assert_int_equal(writes, 0);
}

static void test_revoke(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 1 };
    (void)state;

    assert_int_equal(imx_hab_read_key_status(&key), PB_OK);
    assert_int_equal(imx_hab_revoke_key(&key), PB_OK);
    assert_int_equal(REVOKE, 0x02);
    assert_int_equal(imx_hab_read_key_status(&key), -PB_ERR_KEY_REVOKED);

    /* Again: no write */
    assert_int_equal(imx_hab_revoke_key(&key), PB_OK);
    assert_int_equal(writes, 1);

    /* The other bits stay */
    key.slot = 0;
    assert_int_equal(imx_hab_revoke_key(&key), PB_OK);
    assert_int_equal(REVOKE, 0x03);
}

/* HAB revokes SRK0-2 (F-165) */
static void test_revoke_past_the_srks(void **state)
{
    struct rot_key key = { .id = 0x52c1eda0, .slot = 3 };
    (void)state;

    assert_int_equal(imx_hab_revoke_key(&key), -PB_ERR_PARAM);
    assert_int_equal(writes, 0);

    /* Revoked by an older pb: stays revoked */
    REVOKE = 0x08;
    assert_int_equal(imx_hab_read_key_status(&key), -PB_ERR_KEY_REVOKED);
}

static void test_revoke_not_allowed(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 0 };
    (void)state;

    revoke_allowed_rc = -PB_ERR;
    assert_int_equal(imx_hab_revoke_key(&key), -PB_ERR_STATE);
    assert_int_equal(writes, 0);

    revoke_allowed_rc = PB_OK;
    read_rc = -PB_ERR_IO;
    assert_int_equal(imx_hab_revoke_key(&key), -PB_ERR_IO);
    assert_int_equal(imx_hab_read_key_status(&key), -PB_ERR_IO);
    assert_int_equal(writes, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_not_initialized, setup),
        cmocka_unit_test_setup(test_life_cycle, setup),
        cmocka_unit_test_setup(test_fuse_srk, setup),
        cmocka_unit_test_setup(test_revoke, setup),
        cmocka_unit_test_setup(test_revoke_past_the_srks, setup),
        cmocka_unit_test_setup(test_revoke_not_allowed, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
