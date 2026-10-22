/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * DER helpers (src/lib/der_helpers.c). Inputs that hit the known problems in
 * F-066 (short r/s, a bit string shorter than two bytes) are not tested.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/der_helpers.h>
#include <pb/pb.h>
#include <string.h>

/* SEQUENCE { INTEGER r, INTEGER s } with 32-byte values whose MSB is set, so
 * both have a leading zero */
static uint8_t ecsig[72];

static void make_ecsig(void)
{
    uint8_t *p = ecsig;

    *p++ = 0x30;
    *p++ = 70;
    *p++ = 0x02;
    *p++ = 33;
    *p++ = 0x00;
    memset(p, 0x81, 32);
    p += 32;
    *p++ = 0x02;
    *p++ = 33;
    *p++ = 0x00;
    memset(p, 0x82, 32);
}

static int setup(void **state)
{
    (void)state;
    make_ecsig();
    return 0;
}

static void test_asn1_size_forms(void **state)
{
    static const uint8_t short_form[] = { 0x7f };
    static const uint8_t long1[] = { 0x81, 0xc8 };
    static const uint8_t long2[] = { 0x82, 0x12, 0x34 };
    static const uint8_t long3[] = { 0x83, 0x12, 0x34, 0x56 };
    static const uint8_t long4[] = { 0x84, 0x12, 0x34, 0x56, 0x78 };
    static const uint8_t too_long[] = { 0x85, 0, 0, 0, 0, 1 };
    const unsigned char *p;
    size_t len;
    (void)state;

    p = short_form;
    assert_int_equal(asn1_size(&p, &len), PB_OK);
    assert_int_equal(len, 0x7f);
    assert_ptr_equal(p, short_form + 1);

    p = long1;
    assert_int_equal(asn1_size(&p, &len), PB_OK);
    assert_int_equal(len, 0xc8);
    assert_ptr_equal(p, long1 + 2);

    p = long2;
    assert_int_equal(asn1_size(&p, &len), PB_OK);
    assert_int_equal(len, 0x1234);
    assert_ptr_equal(p, long2 + 3);

    p = long3;
    assert_int_equal(asn1_size(&p, &len), PB_OK);
    assert_int_equal(len, 0x123456);
    assert_ptr_equal(p, long3 + 4);

    p = long4;
    assert_int_equal(asn1_size(&p, &len), PB_OK);
    assert_int_equal(len, 0x12345678);
    assert_ptr_equal(p, long4 + 5);

    p = too_long;
    assert_int_equal(asn1_size(&p, &len), -PB_ERR_ASN1);
}

static void test_ecsig_to_rs(void **state)
{
    uint8_t r[32];
    uint8_t s[32];
    uint8_t expected_r[32];
    uint8_t expected_s[32];
    (void)state;

    memset(expected_r, 0x81, sizeof(expected_r));
    memset(expected_s, 0x82, sizeof(expected_s));

    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), PB_OK);
    assert_memory_equal(r, expected_r, sizeof(r));
    assert_memory_equal(s, expected_s, sizeof(s));
}

static void test_ecsig_to_rs_keeps_leading_zero(void **state)
{
    uint8_t r[33];
    uint8_t s[33];
    (void)state;

    /* Without suppression the 33-byte integers don't fit in 32 bytes */
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, 32, false), -PB_ERR_BUF_TOO_SMALL);

    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), false), PB_OK);
    assert_int_equal(r[0], 0x00);
    assert_int_equal(r[1], 0x81);
    assert_int_equal(s[0], 0x00);
    assert_int_equal(s[1], 0x82);
}

static void test_ecsig_s_too_large(void **state)
{
    uint8_t r[32];
    uint8_t s[32];
    (void)state;

    /* A 33-byte s that has no leading zero to drop */
    ecsig[39] = 0x01;
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), -PB_ERR_BUF_TOO_SMALL);
}

static void test_ecsig_bad_encoding(void **state)
{
    uint8_t r[32];
    uint8_t s[32];
    (void)state;

    ecsig[0] = 0x31;
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[1] = 0x85;
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[2] = 0x03;
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[3] = 0x85;
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[37] = 0x03;
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[38] = 0x85;
    assert_int_equal(der_ecsig_to_rs(ecsig, r, s, sizeof(r), true), -PB_ERR_ASN1);
}

/* SubjectPublicKeyInfo for an EC key: the curve OID, then an uncompressed
 * point of 'point_len' bytes (without the 0x04 prefix) filled with 0xa5 */
static size_t make_ec_key(uint8_t *buf, const uint8_t *curve_oid, size_t oid_len, size_t point_len)
{
    static const uint8_t ec_public_key_oid[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01 };
    size_t bit_string_len = point_len + 2;
    size_t algorithm_len = 2 + sizeof(ec_public_key_oid) + 2 + oid_len;
    size_t bit_string_hdr = (bit_string_len > 127) ? 3 : 2;
    size_t total_len = 2 + algorithm_len + bit_string_hdr + bit_string_len;
    uint8_t *p = buf;

    *p++ = 0x30;
    if (total_len > 127) {
        *p++ = 0x81;
        *p++ = (uint8_t)total_len;
    } else {
        *p++ = (uint8_t)total_len;
    }
    *p++ = 0x30;
    *p++ = (uint8_t)algorithm_len;
    *p++ = 0x06;
    *p++ = sizeof(ec_public_key_oid);
    memcpy(p, ec_public_key_oid, sizeof(ec_public_key_oid));
    p += sizeof(ec_public_key_oid);
    *p++ = 0x06;
    *p++ = (uint8_t)oid_len;
    memcpy(p, curve_oid, oid_len);
    p += oid_len;
    *p++ = 0x03;
    if (bit_string_len > 127)
        *p++ = 0x81;
    *p++ = (uint8_t)bit_string_len;
    *p++ = 0x00; /* Unused bits */
    *p++ = 0x04; /* Uncompressed point */
    memset(p, 0xa5, point_len);
    p += point_len;

    return p - buf;
}

static const uint8_t secp256r1[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07 };
static const uint8_t secp384r1[] = { 0x2b, 0x81, 0x04, 0x00, 0x22 };
static const uint8_t secp521r1[] = { 0x2b, 0x81, 0x04, 0x00, 0x23 };

static void check_ec_key(const uint8_t *oid, size_t oid_len, size_t point_len, dsa_t expected)
{
    uint8_t key[256];
    uint8_t out[160];
    uint8_t expected_point[160];
    dsa_t kind = 0;

    make_ec_key(key, oid, oid_len, point_len);
    memset(expected_point, 0xa5, point_len);

    assert_int_equal(der_ec_public_key_data(key, out, sizeof(out), &kind), PB_OK);
    assert_int_equal(kind, expected);
    assert_memory_equal(out, expected_point, point_len);
}

static void test_ec_public_key_data(void **state)
{
    (void)state;

    check_ec_key(secp256r1, sizeof(secp256r1), 64, DSA_EC_SECP256r1);
    check_ec_key(secp384r1, sizeof(secp384r1), 96, DSA_EC_SECP384r1);
    check_ec_key(secp521r1, sizeof(secp521r1), 132, DSA_EC_SECP521r1);
}

static void test_ec_public_key_output_too_small(void **state)
{
    uint8_t key[256];
    uint8_t out[63];
    dsa_t kind;
    (void)state;

    make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
    assert_int_equal(der_ec_public_key_data(key, out, sizeof(out), &kind), -PB_ERR_BUF_TOO_SMALL);
}

static void test_ec_public_key_unknown_curve(void **state)
{
    static const uint8_t unknown[] = { 0x2b, 0x81, 0x04, 0x00, 0x99 };
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    (void)state;

    make_ec_key(key, unknown, sizeof(unknown), 64);
    assert_int_equal(der_ec_public_key_data(key, out, sizeof(out), &kind), -PB_ERR_NOT_IMPLEMENTED);
}

static void test_ec_public_key_not_ec(void **state)
{
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    (void)state;

    make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
    key[8] = 0x00; /* Last byte of the id-ecPublicKey OID */
    assert_int_equal(der_ec_public_key_data(key, out, sizeof(out), &kind), -PB_ERR_ASN1);
}

static void test_ec_public_key_bad_encoding(void **state)
{
    /* Offsets in the secp256r1 key: outer SEQUENCE, AlgorithmIdentifier
     * SEQUENCE, algorithm OID, curve OID, BIT STRING */
    static const struct {
        size_t offset;
        uint8_t value;
        int rc;
    } cases[] = {
        { 0, 0x31, -PB_ERR_ASN1 },  { 1, 0x85, -PB_ERR_ASN1 },  { 2, 0x31, -PB_ERR_ASN1 },
        { 3, 0x85, -PB_ERR_ASN1 },  { 4, 0x05, -PB_ERR },       { 5, 0x85, -PB_ERR_ASN1 },
        { 13, 0x05, -PB_ERR },      { 14, 0x85, -PB_ERR_ASN1 }, { 23, 0x04, -PB_ERR },
        { 24, 0x85, -PB_ERR_ASN1 },
    };

    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
        key[cases[i].offset] = cases[i].value;
        assert_int_equal(der_ec_public_key_data(key, out, sizeof(out), &kind), cases[i].rc);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_asn1_size_forms),
        cmocka_unit_test_setup(test_ecsig_to_rs, setup),
        cmocka_unit_test_setup(test_ecsig_to_rs_keeps_leading_zero, setup),
        cmocka_unit_test_setup(test_ecsig_s_too_large, setup),
        cmocka_unit_test_setup(test_ecsig_bad_encoding, setup),
        cmocka_unit_test(test_ec_public_key_data),
        cmocka_unit_test(test_ec_public_key_output_too_small),
        cmocka_unit_test(test_ec_public_key_unknown_curve),
        cmocka_unit_test(test_ec_public_key_not_ec),
        cmocka_unit_test(test_ec_public_key_bad_encoding),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
