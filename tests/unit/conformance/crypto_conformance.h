/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Conformance tests for a crypto backend (crypto.md 3, 4 and 9): what a user
 * of hash_*() and dsa_verify() may rely on, whatever the backend below is.
 * The same tests run against every backend that can run on the host: Mbed
 * TLS, and the CAAM behind a model of its job ring.
 *
 * A test program registers the backend under test, and only it, before the
 * tests run, says in crypto_conformance_hashes and _curves which algorithms
 * it registers, and lists the tests with CRYPTO_CONFORMANCE_TESTS(). The
 * tests reach the backend through the crypto API only. Their keys are the
 * internal keystore's (pb_keystore), their tokens and digests are in
 * crypto_vectors.h, none made with the code under test.
 */

#ifndef TESTS_UNIT_CONFORMANCE_CRYPTO_CONFORMANCE_H
#define TESTS_UNIT_CONFORMANCE_CRYPTO_CONFORMANCE_H

#include <pb/crypto.h>

/* What the backend under test registers: HASH_* and DSA_* bits */
extern hash_t crypto_conformance_hashes;
extern dsa_t crypto_conformance_curves;

void crypto_3_known_answers(void **state);
void crypto_3_every_split(void **state);
void crypto_3_1_an_algorithm_the_backend_lacks(void **state);
void crypto_3_5_final_writes_its_room(void **state);
void crypto_3_no_context(void **state);
void crypto_4_a_token_verifies(void **state);
void crypto_4_one_bit_changed(void **state);
void crypto_4_malformed_der(void **state);
void crypto_4_another_curves_signature(void **state);
void crypto_4_a_hash_given_up_at_each_step(void **state);

#define CRYPTO_CONFORMANCE_TESTS(setup, teardown)                                            \
    cmocka_unit_test_setup_teardown(crypto_3_known_answers, setup, teardown),                \
        cmocka_unit_test_setup_teardown(crypto_3_every_split, setup, teardown),              \
        cmocka_unit_test_setup_teardown(                                                     \
            crypto_3_1_an_algorithm_the_backend_lacks, setup, teardown),                     \
        cmocka_unit_test_setup_teardown(crypto_3_5_final_writes_its_room, setup, teardown),  \
        cmocka_unit_test_setup_teardown(crypto_3_no_context, setup, teardown),               \
        cmocka_unit_test_setup_teardown(crypto_4_a_token_verifies, setup, teardown),         \
        cmocka_unit_test_setup_teardown(crypto_4_one_bit_changed, setup, teardown),          \
        cmocka_unit_test_setup_teardown(crypto_4_malformed_der, setup, teardown),            \
        cmocka_unit_test_setup_teardown(crypto_4_another_curves_signature, setup, teardown), \
        cmocka_unit_test_setup_teardown(crypto_4_a_hash_given_up_at_each_step, setup, teardown)

#endif // TESTS_UNIT_CONFORMANCE_CRYPTO_CONFORMANCE_H
