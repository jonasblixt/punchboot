/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8X's fuse dry run (CONFIG_IMX8X_FUSE_DRY_RUN): the fuse, RoT and
 * SLC helpers (src/plat/imx8x/{fusebox,rot_helpers,slc_helpers}.c) with the
 * SCFW's calls they make faked here. A dry run writes no fuse: not a fuse
 * word, not the SECO's revocation commit and not its life cycle forward,
 * which both burn fuses.
 *
 * And the port's plat_rot and plat_slc (platform.md 4.1, 4.2), which the
 * helpers' files define. Built twice: for the i.MX8QXP and for the i.MX8DXL
 * (test_imx8xl_dry_run), whose life cycle can't be locked.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <pb/srk_fuse.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <plat/imx8x/sci/svc/misc/api.h>
#include <plat/imx8x/sci/svc/seco/api.h>
#include <string.h>

#define IPC 1

/* The SoC's fuse words, and the calls that would burn fuses */
static uint32_t fuses[1024];
static unsigned int fuse_writes, commits, forwards;

const uint32_t srk_fuse[IMX8X_SRK_HASH_WORDS] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
};
const size_t srk_fuse_words = IMX8X_SRK_HASH_WORDS;

sc_err_t sc_misc_otp_fuse_read(sc_ipc_t ipc, uint32_t word, uint32_t *val)
{
    assert_int_equal(ipc, IPC);
    assert_true(word < ARRAY_SIZE(fuses));
    *val = fuses[word];
    return SC_ERR_NONE;
}

sc_err_t sc_misc_otp_fuse_write(sc_ipc_t ipc, uint32_t word, uint32_t val)
{
    assert_int_equal(ipc, IPC);
    assert_true(word < ARRAY_SIZE(fuses));
    fuses[word] |= val;
    fuse_writes++;
    return SC_ERR_NONE;
}

sc_err_t sc_seco_commit(sc_ipc_t ipc, uint32_t *info)
{
    assert_int_equal(ipc, IPC);
    /* What the SECO does with a header that revokes SRK 0 */
    fuses[IMX8X_FUSE_REVOKE] |= (1u << 8);
    *info = 0x10;
    commits++;
    return SC_ERR_NONE;
}

sc_err_t sc_seco_forward_lifecycle(sc_ipc_t ipc, uint32_t change)
{
    assert_int_equal(ipc, IPC);
    (void)change;
    forwards++;
    return SC_ERR_NONE;
}

sc_err_t
sc_seco_chip_info(sc_ipc_t ipc, uint16_t *lc, uint16_t *monotonic, uint32_t *uid_l, uint32_t *uid_h)
{
    assert_int_equal(ipc, IPC);
    *lc = 0;
    *monotonic = 0;
    *uid_l = 0;
    *uid_h = 0;
    return SC_ERR_NONE;
}

static int setup(void **state)
{
    (void)state;
    memset(fuses, 0, sizeof(fuses));
    fuse_writes = 0;
    commits = 0;
    forwards = 0;
    imx8x_fuse_init(IPC);
    imx8x_rot_helpers_init(IPC);
    imx8x_slc_helpers_init(IPC);
    return 0;
}

static void test_dry_run_writes_no_fuse(void **state)
{
    (void)state;

    assert_int_equal(imx8x_fuse_write(IMX8X_FUSE_BOOT0, 1), PB_OK);
    assert_int_equal(imx8x_slc_set_configuration(), PB_OK);
    assert_int_equal(fuse_writes, 0);
    assert_int_equal(imx8x_slc_read_status(), SLC_NOT_CONFIGURED);
}

/* F-218: the life cycle's forward to OEM closed went to the SECO */
static void test_dry_run_locks_nothing(void **state)
{
    (void)state;

    fuses[IMX8X_FUSE_SRK0] = 1;
    assert_int_equal(imx8x_slc_read_status(), SLC_CONFIGURATION);

    assert_int_equal(imx8x_slc_set_configuration_locked(), PB_OK);
    assert_int_equal(forwards, 0);
}

/* F-218: the revocation's commit went to the SECO */
static void test_dry_run_revokes_nothing(void **state)
{
    const struct rot_key key = { .id = 0xa90f9680, .slot = 0 };

    (void)state;

    fuses[IMX8X_FUSE_SRK0] = 1;
    assert_int_equal(imx8x_revoke_key(&key), PB_OK);
    assert_int_equal(commits, 0);
    assert_int_equal(fuse_writes, 0);
    assert_int_equal(imx8x_read_key_status(&key), PB_OK);
}

/* platform.md 4.1, 4.2 (F-247): the root of trust and the life cycle are the
 * platform's, AHAB's through the SECO and the SCFW's fuses. What the MEK and
 * the DXL EVK wired themselves until then, unchanged: the DXL's life cycle
 * can't be locked ('slc lock' answers NOT_SUPPORTED, F-212), and neither SoC
 * has EOL (F-263) */
static void test_plat_rot_and_slc(void **state)
{
    (void)state;

    assert_ptr_equal(plat_rot.revoke_key, imx8x_revoke_key);
    assert_ptr_equal(plat_rot.read_key_status, imx8x_read_key_status);

    assert_ptr_equal(plat_slc.read_status, imx8x_slc_read_status);
    assert_ptr_equal(plat_slc.set_configuration, imx8x_slc_set_configuration);
#ifdef CONFIG_PLAT_IMX8XL
    assert_null(plat_slc.set_configuration_locked);
#else
    assert_ptr_equal(plat_slc.set_configuration_locked, imx8x_slc_set_configuration_locked);
#endif
    assert_null(plat_slc.set_eol);
    assert_ptr_equal(plat_slc.fuse_read, imx8x_fuse_read);
    assert_ptr_equal(plat_slc.fuse_write, imx8x_fuse_write);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_dry_run_writes_no_fuse, setup),
        cmocka_unit_test_setup(test_dry_run_locks_nothing, setup),
        cmocka_unit_test_setup(test_dry_run_revokes_nothing, setup),
        cmocka_unit_test_setup(test_plat_rot_and_slc, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
