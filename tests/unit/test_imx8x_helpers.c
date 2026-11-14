/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8X's fuse, RoT and SLC helpers (src/plat/imx8x/{fusebox,
 * rot_helpers,slc_helpers}.c) as the boards build them, without the fuse dry
 * run: the fuse words, the SECO's revocation commit and its life cycle
 * forward reach the SCFW, which the test fakes (test_imx8x_dry_run.c has the
 * dry run). Each SCFW call can fail.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/srk_fuse.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <string.h>

#define IPC 1

/* The SoC's fuse words and the SECO */
static struct {
    uint32_t fuses[1024];
    unsigned int word_writes[1024];
    unsigned int reads, writes, commits, forwards;
    uint32_t forward_change;
    uint16_t lc;
    /* What the commit revokes: the image header's revoke mask */
    uint32_t commit_revokes;
    /* The n-th call fails, 0: none */
    unsigned int read_fails_at;
    unsigned int write_fails_at;
    sc_err_t commit_err;
    sc_err_t forward_err;
    sc_err_t chip_info_err;
} scfw;

const uint32_t srk_fuse[IMX8X_SRK_HASH_WORDS] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};
const size_t srk_fuse_words = IMX8X_SRK_HASH_WORDS;

sc_err_t sc_misc_otp_fuse_read(sc_ipc_t ipc, uint32_t word, uint32_t *val)
{
    assert_int_equal(ipc, IPC);
    assert_true(word < ARRAY_SIZE(scfw.fuses));

    if (++scfw.reads == scfw.read_fails_at)
        return SC_ERR_NOACCESS;

    *val = scfw.fuses[word];
    return SC_ERR_NONE;
}

sc_err_t sc_misc_otp_fuse_write(sc_ipc_t ipc, uint32_t word, uint32_t val)
{
    assert_int_equal(ipc, IPC);
    assert_true(word < ARRAY_SIZE(scfw.fuses));

    if (++scfw.writes == scfw.write_fails_at)
        return SC_ERR_FAIL;

    scfw.fuses[word] |= val;
    scfw.word_writes[word]++;
    return SC_ERR_NONE;
}

sc_err_t sc_seco_commit(sc_ipc_t ipc, uint32_t *info)
{
    assert_int_equal(ipc, IPC);
    assert_int_equal(*info, 0x10); /* The OEM's revocations */
    scfw.commits++;

    if (scfw.commit_err != SC_ERR_NONE)
        return scfw.commit_err;

    scfw.fuses[IMX8X_FUSE_REVOKE] |= scfw.commit_revokes;
    return SC_ERR_NONE;
}

sc_err_t sc_seco_forward_lifecycle(sc_ipc_t ipc, uint32_t change)
{
    assert_int_equal(ipc, IPC);
    scfw.forwards++;
    scfw.forward_change = change;
    return scfw.forward_err;
}

sc_err_t
sc_seco_chip_info(sc_ipc_t ipc, uint16_t *lc, uint16_t *monotonic, uint32_t *uid_l, uint32_t *uid_h)
{
    assert_int_equal(ipc, IPC);

    if (scfw.chip_info_err != SC_ERR_NONE)
        return scfw.chip_info_err;

    *lc = scfw.lc;
    *monotonic = 0;
    *uid_l = 0;
    *uid_h = 0;
    return SC_ERR_NONE;
}

static void init(sc_ipc_t ipc)
{
    imx8x_fuse_init(ipc);
    imx8x_rot_helpers_init(ipc);
    imx8x_slc_helpers_init(ipc);
}

static int setup(void **state)
{
    (void)state;
    memset(&scfw, 0, sizeof(scfw));
    /* SECO's OEM open */
    scfw.lc = 0x8;
    init(IPC);
    return 0;
}

static void fuse_srk(void)
{
    for (unsigned int n = 0; n < IMX8X_SRK_HASH_WORDS; n++)
        scfw.fuses[IMX8X_FUSE_SRK0 + n] = srk_fuse[n];
}

/* imx8x_is_srk_fused(), whose reads work */
static bool srk_fused(void)
{
    bool fused;

    assert_int_equal(imx8x_is_srk_fused(&fused), PB_OK);
    return fused;
}

/* Without the SCFW's handle nothing is read or written */
static void test_no_ipc(void **state)
{
    uint32_t val = 0x1234;
    (void)state;

    init(0);
    assert_int_equal(imx8x_fuse_read(IMX8X_FUSE_BOOT0, &val), -PB_ERR);
    assert_int_equal(val, 0x1234);
    assert_int_equal(imx8x_fuse_write(IMX8X_FUSE_BOOT0, 1), -PB_ERR);
    assert_int_equal(scfw.reads + scfw.writes, 0);
}

static void test_fuse_read_write(void **state)
{
    uint32_t val;
    (void)state;

    assert_int_equal(imx8x_fuse_write(IMX8X_FUSE_BOOT0, 0x42), PB_OK);
    assert_int_equal(scfw.fuses[IMX8X_FUSE_BOOT0], 0x42);
    assert_int_equal(imx8x_fuse_read(IMX8X_FUSE_BOOT0, &val), PB_OK);
    assert_int_equal(val, 0x42);

    scfw.read_fails_at = scfw.reads + 1;
    assert_int_equal(imx8x_fuse_read(IMX8X_FUSE_BOOT0, &val), -PB_ERR_IO);

    scfw.write_fails_at = scfw.writes + 1;
    assert_int_equal(imx8x_fuse_write(IMX8X_FUSE_BOOT1, 1), -PB_ERR_IO);
    assert_int_equal(scfw.fuses[IMX8X_FUSE_BOOT1], 0);
}

/* The SRK hash: 16 words at 730, none of them empty */
static void test_fuse_srk(void **state)
{
    uint32_t other[IMX8X_SRK_HASH_WORDS];
    (void)state;

    assert_int_equal(imx8x_fuse_write_srk(srk_fuse, IMX8X_SRK_HASH_WORDS - 1), -PB_ERR_PARAM);
    memcpy(other, srk_fuse, sizeof(other));
    other[15] = 0;
    assert_int_equal(imx8x_fuse_write_srk(other, IMX8X_SRK_HASH_WORDS), -PB_ERR_PARAM);
    assert_int_equal(scfw.writes, 0);
    assert_false(srk_fused());

    assert_int_equal(imx8x_slc_set_configuration(), PB_OK);
    assert_int_equal(scfw.writes, IMX8X_SRK_HASH_WORDS);

    for (unsigned int n = 0; n < IMX8X_SRK_HASH_WORDS; n++)
        assert_int_equal(scfw.fuses[IMX8X_FUSE_SRK0 + n], srk_fuse[n]);

    assert_true(srk_fused());
}

/* A write that fails stops the hash where it is: the words before it are
 * fused, and the device reads as CONFIGURATION. Fusing again completes it
 * (command-mode.md 5.2) and writes only the words that aren't the hash's
 * yet: what the SCFW does with a word written twice isn't known (F-429) */
static void test_fuse_srk_fails_half_way(void **state)
{
    (void)state;

    scfw.write_fails_at = 5;
    assert_int_equal(imx8x_slc_set_configuration(), -PB_ERR_IO);
    assert_int_equal(scfw.fuses[IMX8X_FUSE_SRK0 + 3], srk_fuse[3]);
    assert_int_equal(scfw.fuses[IMX8X_FUSE_SRK0 + 4], 0);
    assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION);

    scfw.write_fails_at = 0;
    scfw.writes = 0;
    memset(scfw.word_writes, 0, sizeof(scfw.word_writes));
    assert_int_equal(imx8x_slc_set_configuration(), PB_OK);
    assert_int_equal(scfw.writes, IMX8X_SRK_HASH_WORDS - 4);

    for (unsigned int n = 0; n < IMX8X_SRK_HASH_WORDS; n++) {
        assert_int_equal(scfw.word_writes[IMX8X_FUSE_SRK0 + n], n < 4 ? 0 : 1);
        assert_int_equal(scfw.fuses[IMX8X_FUSE_SRK0 + n], srk_fuse[n]);
    }

    /* And once it is complete, nothing */
    scfw.writes = 0;
    assert_int_equal(imx8x_slc_set_configuration(), PB_OK);
    assert_int_equal(scfw.writes, 0);
}

/* A word with some of the hash's bits, from a write that stopped half way
 * through it, is completed: written with the hash's word, as the HABv4
 * driver does (F-200) */
static void test_fuse_srk_completes_a_word(void **state)
{
    (void)state;

    fuse_srk();
    scfw.fuses[IMX8X_FUSE_SRK0 + 9] = 0;
    scfw.fuses[IMX8X_FUSE_SRK0 + 14] = srk_fuse[14] & ~2u; /* 15: 0xf, 0xd */

    assert_int_equal(imx8x_slc_set_configuration(), PB_OK);
    assert_int_equal(scfw.writes, 2);
    assert_int_equal(scfw.word_writes[IMX8X_FUSE_SRK0 + 9], 1);
    assert_int_equal(scfw.word_writes[IMX8X_FUSE_SRK0 + 14], 1);
    assert_int_equal(scfw.fuses[IMX8X_FUSE_SRK0 + 9], srk_fuse[9]);
    assert_int_equal(scfw.fuses[IMX8X_FUSE_SRK0 + 14], srk_fuse[14]);
}

/* Another hash in the fuses, or a word with a bit the hash doesn't have:
 * it can never become the hash, and nothing is written, also not the words
 * before it (-PB_ERR_MEM, as the HABv4 driver, F-200) */
static void test_fuse_srk_over_another_hash(void **state)
{
    (void)state;

    for (unsigned int n = 0; n < IMX8X_SRK_HASH_WORDS; n++)
        scfw.fuses[IMX8X_FUSE_SRK0 + n] = 0x100 + n;

    assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION);
    assert_int_equal(imx8x_slc_set_configuration(), -PB_ERR_MEM);
    assert_int_equal(scfw.writes, 0);
    assert_int_equal(scfw.fuses[IMX8X_FUSE_SRK0], 0x100);

    /* One bit too many, in the last word */
    memset(scfw.fuses, 0, sizeof(scfw.fuses));
    scfw.fuses[IMX8X_FUSE_SRK15] = srk_fuse[15] | 0x80000000;
    assert_int_equal(imx8x_slc_set_configuration(), -PB_ERR_MEM);
    assert_int_equal(scfw.writes, 0);

    /* A word that can't be read: nothing is written either */
    memset(scfw.fuses, 0, sizeof(scfw.fuses));
    scfw.reads = 0;
    scfw.read_fails_at = 7;
    assert_int_equal(imx8x_slc_set_configuration(), -PB_ERR_IO);
    assert_int_equal(scfw.writes, 0);
}

/* Any of the 16 words fused is "fused" */
static void test_srk_fused_by_any_word(void **state)
{
    (void)state;

    for (unsigned int n = 0; n < IMX8X_SRK_HASH_WORDS; n++) {
        memset(scfw.fuses, 0, sizeof(scfw.fuses));
        scfw.fuses[IMX8X_FUSE_SRK0 + n] = 0x80000000;
        assert_true(srk_fused());
    }

    /* The words around the hash aren't it */
    memset(scfw.fuses, 0, sizeof(scfw.fuses));
    scfw.fuses[IMX8X_FUSE_SRK0 - 1] = 1;
    scfw.fuses[IMX8X_FUSE_SRK15 + 1] = 1;
    assert_false(srk_fused());
}

/* F-429: an SRK word that can't be read leaves the life cycle unknown, an
 * error and not a state (security.md 4.4, command-mode.md 3.2). A word read
 * before it that is fused has decided already, and so has a closed SECO */
static void test_srk_read_fails(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 0 };
    bool fused = true;
    (void)state;

    scfw.read_fails_at = 1;
    assert_int_equal(imx8x_is_srk_fused(&fused), -PB_ERR_IO);
    assert_false(fused);

    scfw.reads = 0;
    assert_int_equal(imx8x_slc_read_status(), -PB_ERR_IO);

    /* The second word, after an empty first one */
    scfw.reads = 0;
    scfw.read_fails_at = 2;
    assert_int_equal(imx8x_slc_read_status(), -PB_ERR_IO);

    /* Or the last one */
    scfw.reads = 0;
    scfw.read_fails_at = IMX8X_SRK_HASH_WORDS;
    assert_int_equal(imx8x_slc_read_status(), -PB_ERR_IO);

    scfw.reads = 0;
    scfw.read_fails_at = 2;
    scfw.fuses[IMX8X_FUSE_SRK0] = 1;
    assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION);

    scfw.reads = 0;
    scfw.read_fails_at = 1;
    scfw.lc = 0x80;
    assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION_LOCKED);

    /* A revocation isn't committed on a hash that can't be read */
    scfw.fuses[IMX8X_FUSE_SRK0] = 0;
    scfw.fuses[IMX8X_FUSE_SRK15] = 1;
    scfw.commit_revokes = 1u << 8;
    scfw.reads = 0;
    scfw.read_fails_at = 3;
    assert_int_equal(imx8x_revoke_key(&key), -PB_ERR_IO);
    assert_int_equal(scfw.commits, 0);
}

/* The life cycle: SECO's bits 6-10 are closed, whatever the SRK fuses say.
 * Below them the SRK fuses decide */
static void test_life_cycle(void **state)
{
    (void)state;

    assert_int_equal(imx8x_slc_read_status(), SLC_NOT_CONFIGURED);
    fuse_srk();
    assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION);

    for (unsigned int bit = 0; bit < 16; bit++) {
        scfw.lc = 1u << bit;

        if (bit >= 6 && bit <= 10)
            assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION_LOCKED);
        else
            assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION);
    }

    memset(scfw.fuses, 0, sizeof(scfw.fuses));
    scfw.lc = 0x80;
    assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION_LOCKED);

    /* No answer from the SECO: an error, not a state */
    scfw.chip_info_err = SC_ERR_IPC;
    assert_int_equal(imx8x_slc_read_status(), -PB_ERR_IO);
}

/* The lock forwards SECO's life cycle to OEM closed, 0x10 */
static void test_lock(void **state)
{
    (void)state;

    fuse_srk();
    assert_int_equal(imx8x_slc_set_configuration_locked(), PB_OK);
    assert_int_equal(scfw.forwards, 1);
    assert_int_equal(scfw.forward_change, 16);

    scfw.forward_err = SC_ERR_NOACCESS;
    assert_int_equal(imx8x_slc_set_configuration_locked(), -PB_ERR_IO);
    assert_int_equal(scfw.forwards, 2);
}

/* The key state: SRK n is revoked by bit 8 + n of the revoke word. Slots
 * past the four SRKs read as not revoked */
static void test_key_status(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 0 };
    (void)state;

    for (key.slot = 0; key.slot < 8; key.slot++)
        assert_int_equal(imx8x_read_key_status(&key), PB_OK);

    scfw.fuses[IMX8X_FUSE_REVOKE] = 0xffff00ff & ~(1u << 9);
    key.slot = 0;
    assert_int_equal(imx8x_read_key_status(&key), PB_OK);

    scfw.fuses[IMX8X_FUSE_REVOKE] = (1u << 8) | (1u << 11);
    key.slot = 0;
    assert_int_equal(imx8x_read_key_status(&key), -PB_ERR_KEY_REVOKED);
    key.slot = 1;
    assert_int_equal(imx8x_read_key_status(&key), PB_OK);
    key.slot = 3;
    assert_int_equal(imx8x_read_key_status(&key), -PB_ERR_KEY_REVOKED);

    /* Bits 12-15 aren't SRKs: slot 4 is never revoked */
    scfw.fuses[IMX8X_FUSE_REVOKE] = 0xf000;
    key.slot = 4;
    assert_int_equal(imx8x_read_key_status(&key), PB_OK);

    scfw.read_fails_at = scfw.reads + 1;
    key.slot = 0;
    assert_int_equal(imx8x_read_key_status(&key), -PB_ERR_IO);
}

/* The revocation: the SECO commits what the running image's header
 * revokes, and the key's bit must be set afterwards */
static void test_revoke(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 1 };
    (void)state;

    fuse_srk();
    scfw.commit_revokes = 1u << 9;
    assert_int_equal(imx8x_revoke_key(&key), PB_OK);
    assert_int_equal(scfw.commits, 1);
    assert_int_equal(imx8x_read_key_status(&key), -PB_ERR_KEY_REVOKED);

    /* Again: already revoked, no commit */
    assert_int_equal(imx8x_revoke_key(&key), PB_OK);
    assert_int_equal(scfw.commits, 1);
}

static void test_revoke_refused(void **state)
{
    struct rot_key key = { .id = 0xa90f9680, .slot = 4 };
    (void)state;

    /* Four SRKs */
    assert_int_equal(imx8x_revoke_key(&key), -PB_ERR_PARAM);

    /* Not before the SRK hash is fused */
    key.slot = 0;
    assert_int_equal(imx8x_revoke_key(&key), -PB_ERR);
    assert_int_equal(scfw.commits, 0);

    fuse_srk();

    /* The revoke word can't be read: the first read after the hash */
    scfw.reads = 0;
    scfw.read_fails_at = 2;
    assert_int_equal(imx8x_revoke_key(&key), -PB_ERR_IO);
    assert_int_equal(scfw.commits, 0);
    scfw.read_fails_at = 0;

    /* The SECO refuses the commit */
    scfw.commit_err = SC_ERR_NOACCESS;
    assert_int_equal(imx8x_revoke_key(&key), -PB_ERR_IO);
    assert_int_equal(scfw.commits, 1);
    scfw.commit_err = SC_ERR_NONE;

    /* The header revokes another key: committed, and an error. The other
     * key is revoked now */
    scfw.commit_revokes = 1u << 10;
    assert_int_equal(imx8x_revoke_key(&key), -PB_ERR_STATE);
    assert_int_equal(scfw.commits, 2);
    key.slot = 2;
    assert_int_equal(imx8x_read_key_status(&key), -PB_ERR_KEY_REVOKED);
    key.slot = 0;
    assert_int_equal(imx8x_read_key_status(&key), PB_OK);

    /* The revoke word can't be read after the commit */
    scfw.commit_revokes = 1u << 8;
    scfw.reads = 0;
    scfw.read_fails_at = 3;
    assert_int_equal(imx8x_revoke_key(&key), -PB_ERR_IO);
    assert_int_equal(scfw.commits, 3);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_no_ipc, setup),
        cmocka_unit_test_setup(test_fuse_read_write, setup),
        cmocka_unit_test_setup(test_fuse_srk, setup),
        cmocka_unit_test_setup(test_fuse_srk_fails_half_way, setup),
        cmocka_unit_test_setup(test_fuse_srk_completes_a_word, setup),
        cmocka_unit_test_setup(test_fuse_srk_over_another_hash, setup),
        cmocka_unit_test_setup(test_srk_fused_by_any_word, setup),
        cmocka_unit_test_setup(test_srk_read_fails, setup),
        cmocka_unit_test_setup(test_life_cycle, setup),
        cmocka_unit_test_setup(test_lock, setup),
        cmocka_unit_test_setup(test_key_status, setup),
        cmocka_unit_test_setup(test_revoke, setup),
        cmocka_unit_test_setup(test_revoke_refused, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
