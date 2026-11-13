/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Conformance tests for a block device (bio.md 3 and 8): what a user of
 * bio_read() and bio_write() may rely on, whatever the driver below is. The
 * same tests run against every device that can run on the host, the tests'
 * fakes too, so that none of them is more forgiving than a driver.
 *
 * A test program sets bio_conformance_dev in its setup, to a registered
 * device of at least BIO_CONFORMANCE_MIN_BYTES whose content may be
 * overwritten, and lists the tests with BIO_CONFORMANCE_TESTS(). The tests
 * reach the medium through bio only.
 */

#ifndef TESTS_UNIT_CONFORMANCE_BIO_CONFORMANCE_H
#define TESTS_UNIT_CONFORMANCE_BIO_CONFORMANCE_H

#include <pb/bio.h>

#define BIO_CONFORMANCE_MIN_BYTES (64 * 1024)

/* The device under test */
extern bio_dev_t bio_conformance_dev;

/* Called at the end of each test, when set: what the test program checks of
 * its own, a card model's violation log for one */
extern void (*bio_conformance_after)(void);

void bio_3_1_a_read_that_isnt_whole_blocks_is_trimmed(void **state);
void bio_3_2_a_write_that_isnt_whole_blocks_is_padded_with_zeros(void **state);
void bio_3_a_length_of_0_does_nothing(void **state);
void bio_3_a_range_outside_the_device_is_refused(void **state);
void bio_3_any_length_in_one_call(void **state);

#define BIO_CONFORMANCE_TESTS(setup, teardown)                                              \
    cmocka_unit_test_setup_teardown(                                                        \
        bio_3_1_a_read_that_isnt_whole_blocks_is_trimmed, setup, teardown),                 \
        cmocka_unit_test_setup_teardown(                                                    \
            bio_3_2_a_write_that_isnt_whole_blocks_is_padded_with_zeros, setup, teardown),  \
        cmocka_unit_test_setup_teardown(bio_3_a_length_of_0_does_nothing, setup, teardown), \
        cmocka_unit_test_setup_teardown(                                                    \
            bio_3_a_range_outside_the_device_is_refused, setup, teardown),                  \
        cmocka_unit_test_setup_teardown(bio_3_any_length_in_one_call, setup, teardown)

#endif // TESTS_UNIT_CONFORMANCE_BIO_CONFORMANCE_H
