/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Conformance tests for a block device (bio.md 3 and 8): what a user of
 * bio_read(), bio_write(), bio_erase() and bio_write_done() may rely on,
 * whatever the driver below is. The same tests run against every device
 * that can run on the host, the tests' fakes too, so that none of them is
 * more forgiving than a driver.
 *
 * A test program sets, in its setup, bio_conformance_dev to a registered
 * device of at least BIO_CONFORMANCE_MIN_BYTES whose content may be
 * overwritten, and the other bio_conformance_* to what that device has, and
 * lists the tests with BIO_CONFORMANCE_TESTS(). The tests reach the medium
 * through bio only. On a device with BIO_FLAG_ERASE_BEFORE_WRITE they erase
 * before they write, as every user does today (bio.md 3.3).
 */

#ifndef TESTS_UNIT_CONFORMANCE_BIO_CONFORMANCE_H
#define TESTS_UNIT_CONFORMANCE_BIO_CONFORMANCE_H

#include <pb/bio.h>
#include <stdbool.h>

#define BIO_CONFORMANCE_MIN_BYTES (64 * 1024)

/* The device under test */
extern bio_dev_t bio_conformance_dev;

/* Called at the end of each test, when set: what the test program checks of
 * its own, a card model's violation log for one */
extern void (*bio_conformance_after)(void);

/* The device has an erase (bio.md 3.3). One with
 * BIO_FLAG_ERASE_BEFORE_WRITE has to */
extern bool bio_conformance_erases;

/* How the test program makes the medium fail a write (bio.md 3.2), NULL
 * when it can't: the test of a write that fails is skipped */
struct bio_conformance_fault {
    /* From the next write on, which starts at block 'lba' of the device,
     * every block after the first 'blocks' fails */
    void (*fail_writes)(lba_t lba, size_t blocks);
    /* The medium works again */
    void (*end)(void);
};

extern const struct bio_conformance_fault *bio_conformance_fault;

void bio_3_1_a_read_that_isnt_whole_blocks_is_trimmed(void **state);
void bio_3_2_a_write_that_isnt_whole_blocks_is_padded_with_zeros(void **state);
void bio_3_2_a_write_that_fails_half_way(void **state);
void bio_3_3_an_erase_erases_exactly_the_blocks(void **state);
void bio_3_3_an_erase_of_0_blocks_or_outside_the_device(void **state);
void bio_3_4_write_done_keeps_what_was_written(void **state);
void bio_3_a_length_of_0_does_nothing(void **state);
void bio_3_a_range_outside_the_device_is_refused(void **state);
void bio_3_any_length_in_one_call(void **state);

#define BIO_CONFORMANCE_TESTS(setup, teardown)                                                 \
    cmocka_unit_test_setup_teardown(                                                           \
        bio_3_1_a_read_that_isnt_whole_blocks_is_trimmed, setup, teardown),                    \
        cmocka_unit_test_setup_teardown(                                                       \
            bio_3_2_a_write_that_isnt_whole_blocks_is_padded_with_zeros, setup, teardown),     \
        cmocka_unit_test_setup_teardown(bio_3_2_a_write_that_fails_half_way, setup, teardown), \
        cmocka_unit_test_setup_teardown(                                                       \
            bio_3_3_an_erase_erases_exactly_the_blocks, setup, teardown),                      \
        cmocka_unit_test_setup_teardown(                                                       \
            bio_3_3_an_erase_of_0_blocks_or_outside_the_device, setup, teardown),              \
        cmocka_unit_test_setup_teardown(                                                       \
            bio_3_4_write_done_keeps_what_was_written, setup, teardown),                       \
        cmocka_unit_test_setup_teardown(bio_3_a_length_of_0_does_nothing, setup, teardown),    \
        cmocka_unit_test_setup_teardown(                                                       \
            bio_3_a_range_outside_the_device_is_refused, setup, teardown),                     \
        cmocka_unit_test_setup_teardown(bio_3_any_length_in_one_call, setup, teardown)

#endif // TESTS_UNIT_CONFORMANCE_BIO_CONFORMANCE_H
