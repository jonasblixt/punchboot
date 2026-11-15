/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A build that writes no fuses, CONFIG_SLC_NO_FUSE_WRITES (system.md 4,
 * D-087), with CONFIG_SLC_EOL on: the life cycle (src/slc.c) and the keys'
 * revocation (src/rot.c) call none of the port's writes, in any state, and
 * answer as a port without them. The reads are as without the option.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/keys.h>
#include <pb/pb.h>
#include <pb/rot.h>
#include <pb/slc.h>

#ifndef CONFIG_SLC_NO_FUSE_WRITES
#error "This test is built with CONFIG_SLC_NO_FUSE_WRITES"
#endif
#ifndef CONFIG_SLC_EOL
#error "This test is built with CONFIG_SLC_EOL: the option wins over it"
#endif

#define KEY_A 0xa90f9680

const struct pb_key pb_keys[] = {
    { .id = KEY_A, .kind = DSA_EC_SECP256r1, .slot = 0 },
};
const unsigned int pb_no_of_keys = 1;

static slc_t slc_state;
static int writes; /* Every call of a port's write */
static int reads;

static slc_t fake_read_status(void)
{
    reads++;
    return slc_state;
}

static int fake_write(void)
{
    writes++;
    return PB_OK;
}

static int fake_fuse_read(uint32_t index, uint32_t *value)
{
    (void)index;
    *value = 0;
    return PB_OK;
}

static int fake_fuse_write(uint32_t index, uint32_t value)
{
    (void)index;
    (void)value;
    writes++;
    return PB_OK;
}

static int fake_revoke_key(const struct rot_key *key)
{
    (void)key;
    writes++;
    return PB_OK;
}

static int fake_read_key_status(const struct rot_key *key)
{
    (void)key;
    return PB_OK;
}

/* A port that can write everything, and a board with a configuration fuse */
static const struct slc_ops slc_cfg = {
    .read_status = fake_read_status,
    .set_configuration = fake_write,
    .set_configuration_locked = fake_write,
    .set_eol = fake_write,
    .fuse_read = fake_fuse_read,
    .fuse_write = fake_fuse_write,
};

static const struct slc_fuse board_fuses[] = {
    { 7, 0x2060 },
};

static const struct rot_config rot_cfg = {
    .revoke_key = fake_revoke_key,
    .read_key_status = fake_read_key_status,
};

static void test_no_write_in_any_state(void **state)
{
    static const slc_t states[] = {
        SLC_NOT_CONFIGURED,
        SLC_CONFIGURATION,
        SLC_CONFIGURATION_LOCKED,
        SLC_EOL,
        -PB_ERR_IO, /* Unknown: the build's answer, not the read's */
    };
    (void)state;

    assert_int_equal(slc_init(&slc_cfg, board_fuses, ARRAY_SIZE(board_fuses)), PB_OK);
    assert_int_equal(rot_init(&rot_cfg), PB_OK);

    for (size_t i = 0; i < ARRAY_SIZE(states); i++) {
        slc_state = states[i];
        writes = 0;
        reads = 0;

        assert_int_equal(slc_set_configuration(), -PB_ERR_NOT_SUPPORTED);
        assert_int_equal(slc_set_configuration_locked(), -PB_ERR_NOT_SUPPORTED);
        assert_int_equal(slc_set_eol(), -PB_ERR_NOT_SUPPORTED);
        assert_int_equal(rot_revoke_key(KEY_A), -PB_ERR_NOT_SUPPORTED);
        assert_int_equal(writes, 0);
        /* Nothing is read first */
        assert_int_equal(reads, 0);

        /* The life cycle and the keys still read */
        assert_int_equal(slc_read_status(), states[i]);
        assert_int_equal(rot_read_key_status(KEY_A), PB_OK);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_no_write_in_any_state),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
