/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The boot source policies without state (boot.md 3.5): fixed
 * (src/boot/policy_fixed.c) and staggered chain (src/boot/policy_chain.c),
 * and the core's calls to the board's policy (src/boot/policy.c). A/B is in
 * test_ab_state.c, the core's loop over the candidates in test_boot.c.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/policy.h>
#include <boot/policy_chain.h>
#include <boot/policy_fixed.h>
#include <pb/bio.h>
#include <pb/pb.h>
#include <stdlib.h>
#include <string.h>

#include "fakes/ramdisk.h"

#define DISK_BLOCKS 64

#define UU_DISK \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define UU_SYS_A \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define UU_SYS_B \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define UU_RESCUE \
    (const unsigned char *)"\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
#define UU_DATA \
    (const unsigned char *)"\x65\x6a\xb3\xfc\x58\x56\x4a\x5e\xa2\xae\x5a\x01\x83\x13\xb3\xee"
#define UU_OTHER \
    (const unsigned char *)"\xff\x4d\xdc\x6c\xad\x7a\x47\xe8\x87\x73\x67\x29\x39\x2d\xd1\xb5"

/* The objects the way a board has them in its data: the macros are constant
 * initializers */
static const struct boot_policy fixed = BOOT_POLICY_FIXED(UU_SYS_A);

static const unsigned char *const chain_parts[] = { UU_SYS_A, UU_SYS_B, UU_RESCUE };
static const struct boot_policy chain = BOOT_POLICY_CHAIN(chain_parts);

static const unsigned char *const one_part[] = { UU_SYS_B };
static const struct boot_policy chain_of_one = BOOT_POLICY_CHAIN(one_part);

struct fixture {
    struct ramdisk *disk;
    bio_dev_t sys_a;
    bio_dev_t sys_b;
    bio_dev_t rescue;
    bio_dev_t data;
};

static int setup(void **state)
{
    struct fixture *f = calloc(1, sizeof(*f));
    assert_non_null(f);
    f->disk = ramdisk_new(DISK_BLOCKS);
    assert_non_null(f->disk);

    bio_reset();
    bio_dev_t disk = ramdisk_attach(f->disk, UU_DISK);
    assert_true(disk >= 0);
    f->sys_a = bio_allocate_parent(disk, 0, 9, RAMDISK_BLOCK_SIZE, UU_SYS_A, "System A");
    f->sys_b = bio_allocate_parent(disk, 10, 19, RAMDISK_BLOCK_SIZE, UU_SYS_B, "System B");
    f->rescue = bio_allocate_parent(disk, 20, 29, RAMDISK_BLOCK_SIZE, UU_RESCUE, "Rescue");
    f->data = bio_allocate_parent(disk, 30, 39, RAMDISK_BLOCK_SIZE, UU_DATA, "Data");
    assert_true(f->sys_a >= 0 && f->sys_b >= 0 && f->rescue >= 0 && f->data >= 0);

    *state = f;
    return 0;
}

static int teardown(void **state)
{
    struct fixture *f = *state;
    ramdisk_free(f->disk);
    free(f);
    return 0;
}

static bool bootable(bio_dev_t dev)
{
    return (bio_get_flags(dev) & BIO_FLAG_BOOTABLE) != 0;
}

/* The next candidate is 'expected' */
static void assert_next(const struct boot_policy *policy, const unsigned char *expected)
{
    uuid_t uu;

    memset(uu, 0xff, sizeof(uu));
    assert_int_equal(policy->ops->next(uu), PB_OK);
    assert_memory_equal(uu, expected, 16);
}

/* ... and it fails to load or verify */
static void assert_next_fails(const struct boot_policy *policy, const unsigned char *expected)
{
    assert_next(policy, expected);
    policy->ops->report(expected, -PB_ERR_BAD_PAYLOAD);
}

/* ... and the boot goes on with it */
static void assert_next_boots(const struct boot_policy *policy, const unsigned char *expected)
{
    assert_next(policy, expected);
    policy->ops->report(expected, PB_OK);
}

static void assert_none_left(const struct boot_policy *policy)
{
    uuid_t uu;

    assert_int_equal(policy->ops->next(uu), -PB_ERR_NOT_FOUND);
}

/* Runs first: the modules keep their configuration in statics */
static void test_not_initialized(void **state)
{
    uuid_t uu;
    (void)state;

    assert_int_equal(fixed.ops->next(uu), -PB_ERR_STATE);
    assert_int_equal(chain.ops->next(uu), -PB_ERR_STATE);

    memset(uu, 0xff, sizeof(uu));
    fixed.ops->status(uu);
    assert_true(uuid_is_null(uu));
    memset(uu, 0xff, sizeof(uu));
    chain.ops->status(uu);
    assert_true(uuid_is_null(uu));

    assert_int_equal(fixed.ops->mode(), BOOT_POLICY_MODE_NORMAL);
    assert_int_equal(chain.ops->mode(), BOOT_POLICY_MODE_NORMAL);
}

static void test_fixed_object(void **state)
{
    (void)state;

    assert_ptr_equal(fixed.ops, &boot_policy_fixed_ops);
    assert_memory_equal(fixed.config, UU_SYS_A, 16);
    assert_int_equal(fixed.length, 0);
}

/* One partition: it is the only one that is bootable, and each boot's only
 * candidate */
static void test_fixed(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    assert_int_equal(fixed.ops->init(&fixed), PB_OK);
    assert_true(bootable(f->sys_a));
    assert_false(bootable(f->sys_b));
    assert_false(bootable(f->rescue));
    assert_false(bootable(f->data));
    assert_false(bootable(bio_get_part_by_uu(UU_DISK)));

    assert_next_boots(&fixed, UU_SYS_A);
    assert_int_equal(fixed.ops->mode(), BOOT_POLICY_MODE_NORMAL);
    /* The next boot (a boot that late_boot stopped, then command mode's) */
    assert_next_boots(&fixed, UU_SYS_A);

    /* It fails: nothing else to try, and the next boot tries it again */
    assert_next_fails(&fixed, UU_SYS_A);
    assert_none_left(&fixed);
    assert_next_fails(&fixed, UU_SYS_A);
    assert_none_left(&fixed);
    assert_next_boots(&fixed, UU_SYS_A);

    /* No state: nothing was written */
    assert_int_equal(f->disk->writes, 0);

    /* 'boot status' is its partition, 'boot activate' isn't supported */
    memset(uu, 0xff, sizeof(uu));
    fixed.ops->status(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);
    assert_int_equal(fixed.ops->activate((unsigned char *)UU_SYS_B), -PB_ERR_NOT_SUPPORTED);
    assert_int_equal(fixed.ops->activate((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
    uuid_clear(uu);
    assert_int_equal(fixed.ops->activate(uu), -PB_ERR_NOT_SUPPORTED);
    assert_next_boots(&fixed, UU_SYS_A);

    /* init can run again (a table that is installed, boot.md 3.1) */
    assert_next(&fixed, UU_SYS_A);
    assert_int_equal(fixed.ops->init(&fixed), PB_OK);
    assert_next_boots(&fixed, UU_SYS_A);
}

static void test_fixed_init_errors(void **state)
{
    static const struct boot_policy no_part = { .ops = &boot_policy_fixed_ops };
    static const struct boot_policy unknown = BOOT_POLICY_FIXED(UU_OTHER);
    struct fixture *f = *state;
    uuid_t uu;

    /* A good one first, to see that a failed init leaves nothing of it */
    assert_int_equal(fixed.ops->init(&fixed), PB_OK);

    assert_int_equal(no_part.ops->init(&no_part), -PB_ERR_PARAM);
    assert_int_equal(fixed.ops->next(uu), -PB_ERR_STATE);

    /* A partition that isn't in the table (board.md 2.1) */
    assert_int_equal(unknown.ops->init(&unknown), -PB_ERR_NOT_FOUND);
    assert_int_equal(unknown.ops->next(uu), -PB_ERR_STATE);
    memset(uu, 0xff, sizeof(uu));
    unknown.ops->status(uu);
    assert_true(uuid_is_null(uu));
    assert_false(bootable(f->sys_b));
}

static void test_chain_object(void **state)
{
    (void)state;

    assert_ptr_equal(chain.ops, &boot_policy_chain_ops);
    assert_ptr_equal(chain.config, chain_parts);
    assert_int_equal(chain.length, 3);
    assert_int_equal(chain_of_one.length, 1);
}

/* The list in the board's order: the next one when one fails */
static void test_chain(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    assert_int_equal(chain.ops->init(&chain), PB_OK);
    assert_true(bootable(f->sys_a));
    assert_true(bootable(f->sys_b));
    assert_true(bootable(f->rescue));
    assert_false(bootable(f->data));
    assert_false(bootable(bio_get_part_by_uu(UU_DISK)));

    /* The common path: the first one */
    assert_next_boots(&chain, UU_SYS_A);
    assert_int_equal(chain.ops->mode(), BOOT_POLICY_MODE_NORMAL);
    assert_next_boots(&chain, UU_SYS_A);

    /* A fails: B, as a fallback */
    assert_next_fails(&chain, UU_SYS_A);
    assert_int_equal(chain.ops->mode(), BOOT_POLICY_MODE_NORMAL);
    assert_next_boots(&chain, UU_SYS_B);
    /* The handover reads the mode after the report */
    assert_int_equal(chain.ops->mode(), BOOT_POLICY_MODE_FALLBACK);

    /* No state: the next boot starts with A again */
    assert_next_fails(&chain, UU_SYS_A);
    assert_next_fails(&chain, UU_SYS_B);
    assert_next_boots(&chain, UU_RESCUE);
    assert_int_equal(chain.ops->mode(), BOOT_POLICY_MODE_FALLBACK);

    /* All of them fail: none left, then from the start */
    assert_next_fails(&chain, UU_SYS_A);
    assert_next_fails(&chain, UU_SYS_B);
    assert_next_fails(&chain, UU_RESCUE);
    assert_none_left(&chain);
    assert_next_boots(&chain, UU_SYS_A);
    assert_int_equal(chain.ops->mode(), BOOT_POLICY_MODE_NORMAL);

    assert_int_equal(f->disk->writes, 0);

    /* 'boot status' is the first of the list, 'boot activate' isn't
     * supported */
    memset(uu, 0xff, sizeof(uu));
    chain.ops->status(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);
    assert_int_equal(chain.ops->activate((unsigned char *)UU_SYS_B), -PB_ERR_NOT_SUPPORTED);
    assert_next_boots(&chain, UU_SYS_A);

    /* init starts the candidates over */
    assert_next_fails(&chain, UU_SYS_A);
    assert_int_equal(chain.ops->init(&chain), PB_OK);
    assert_next_boots(&chain, UU_SYS_A);
    assert_int_equal(chain.ops->mode(), BOOT_POLICY_MODE_NORMAL);
}

static void test_chain_of_one(void **state)
{
    struct fixture *f = *state;

    assert_int_equal(chain_of_one.ops->init(&chain_of_one), PB_OK);
    assert_false(bootable(f->sys_a));
    assert_true(bootable(f->sys_b));

    assert_next_fails(&chain_of_one, UU_SYS_B);
    assert_none_left(&chain_of_one);
    assert_next_boots(&chain_of_one, UU_SYS_B);
    assert_int_equal(chain_of_one.ops->mode(), BOOT_POLICY_MODE_NORMAL);
}

static void test_chain_init_errors(void **state)
{
    static const unsigned char *const with_unknown[] = { UU_SYS_A, UU_OTHER, UU_SYS_B };
    static const unsigned char *const with_null[] = { UU_SYS_A, NULL };
    static const struct boot_policy unknown = BOOT_POLICY_CHAIN(with_unknown);
    static const struct boot_policy null_part = BOOT_POLICY_CHAIN(with_null);
    static const struct boot_policy no_list = { .ops = &boot_policy_chain_ops, .length = 2 };
    static const struct boot_policy empty = { .ops = &boot_policy_chain_ops,
                                              .config = chain_parts };
    uuid_t uu;
    (void)state;

    /* At least one partition */
    assert_int_equal(empty.ops->init(&empty), -PB_ERR_PARAM);
    assert_int_equal(no_list.ops->init(&no_list), -PB_ERR_PARAM);
    assert_int_equal(null_part.ops->init(&null_part), -PB_ERR_PARAM);

    /* A partition that isn't in the table (board.md 2.1) */
    assert_int_equal(chain.ops->init(&chain), PB_OK);
    assert_int_equal(unknown.ops->init(&unknown), -PB_ERR_NOT_FOUND);
    /* A failed init leaves no policy: nothing is handed out */
    assert_int_equal(chain.ops->next(uu), -PB_ERR_STATE);
    memset(uu, 0xff, sizeof(uu));
    chain.ops->status(uu);
    assert_true(uuid_is_null(uu));
}

/* A policy's names for its systems, as A/B has them */
static const char *system_name(uuid_t part_uu)
{
    return uuid_compare(part_uu, UU_SYS_B) == 0 ? "B" : "?";
}

/* The core's calls reach the board's policy */
static void test_core_calls(void **state)
{
    struct fixture *f = *state;
    uuid_t uu;

    assert_int_equal(boot_policy_init(&chain), PB_OK);
    assert_true(bootable(f->sys_a));

    assert_int_equal(boot_policy_next(uu), PB_OK);
    assert_memory_equal(uu, UU_SYS_A, 16);
    assert_int_equal(boot_policy_mode(), BOOT_POLICY_MODE_NORMAL);
    boot_policy_report(uu, -PB_ERR_BAD_PAYLOAD);
    assert_int_equal(boot_policy_next(uu), PB_OK);
    assert_memory_equal(uu, UU_SYS_B, 16);
    assert_int_equal(boot_policy_mode(), BOOT_POLICY_MODE_FALLBACK);
    boot_policy_report(uu, PB_OK);

    memset(uu, 0xff, sizeof(uu));
    boot_policy_status(uu);
    assert_memory_equal(uu, UU_SYS_A, 16);
    assert_int_equal(boot_policy_activate((unsigned char *)UU_SYS_B), -PB_ERR_NOT_SUPPORTED);

    /* The staggered chain and the fixed policy have no systems, so no name
     * for one (boot.md 5.6): the handover then has no pb,active-system */
    assert_null(boot_policy_system_name((unsigned char *)UU_SYS_A));
    assert_int_equal(boot_policy_init(&fixed), PB_OK);
    assert_null(boot_policy_system_name((unsigned char *)UU_SYS_A));

    /* A policy with names is asked for any UUID */
    static const struct boot_policy_ops named_ops = { .system_name = system_name };
    static const struct boot_policy named = { .ops = &named_ops };

    boot_policy_set(&named);
    assert_string_equal(boot_policy_system_name((unsigned char *)UU_SYS_B), "B");
    assert_string_equal(boot_policy_system_name((unsigned char *)UU_OTHER), "?");

    /* The policy's error is init's, and the policy stays the board's */
    static const struct boot_policy unknown = BOOT_POLICY_FIXED(UU_OTHER);

    assert_int_equal(boot_policy_init(&unknown), -PB_ERR_NOT_FOUND);
    assert_int_equal(boot_policy_next(uu), -PB_ERR_STATE);

    /* boot_init() only names the policy: it isn't set up again */
    assert_int_equal(boot_policy_init(&fixed), PB_OK);
    assert_int_equal(boot_policy_next(uu), PB_OK);
    boot_policy_set(&fixed);
    assert_int_equal(boot_policy_next(uu), -PB_ERR_NOT_FOUND);
}

/* A board without a policy: nothing boots from storage, nothing to
 * activate, no partition */
static void test_core_calls_without_a_policy(void **state)
{
    static const struct boot_policy none;
    const struct boot_policy *policies[] = { &none, NULL };
    uuid_t uu;
    (void)state;

    for (size_t i = 0; i < 2; i++) {
        if (i == 0)
            assert_int_equal(boot_policy_init(policies[i]), PB_OK);
        else
            boot_policy_set(policies[i]);

        assert_int_equal(boot_policy_next(uu), -PB_ERR_NOT_SUPPORTED);
        boot_policy_report(UU_SYS_A, PB_OK);
        assert_int_equal(boot_policy_mode(), BOOT_POLICY_MODE_NORMAL);
        assert_int_equal(boot_policy_activate((unsigned char *)UU_SYS_A), -PB_ERR_NOT_SUPPORTED);
        memset(uu, 0xff, sizeof(uu));
        boot_policy_status(uu);
        assert_true(uuid_is_null(uu));
        assert_null(boot_policy_system_name((unsigned char *)UU_SYS_A));
    }

    assert_int_equal(boot_policy_init(NULL), PB_OK);
    assert_int_equal(boot_policy_next(uu), -PB_ERR_NOT_SUPPORTED);
}

#define POLICY_TEST(f) cmocka_unit_test_setup_teardown(f, setup, teardown)

int main(void)
{
    const struct CMUnitTest tests[] = {
        POLICY_TEST(test_not_initialized), POLICY_TEST(test_fixed_object),
        POLICY_TEST(test_fixed),           POLICY_TEST(test_fixed_init_errors),
        POLICY_TEST(test_chain_object),    POLICY_TEST(test_chain),
        POLICY_TEST(test_chain_of_one),    POLICY_TEST(test_chain_init_errors),
        POLICY_TEST(test_core_calls),      POLICY_TEST(test_core_calls_without_a_policy),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
