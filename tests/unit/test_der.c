/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * DER helpers (src/lib/der_helpers.c).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <bpak/keystore.h>
#include <pb/der_helpers.h>
#include <pb/pb.h>
#include <stdlib.h>
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

    /* The leading zeros are dropped */
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), PB_OK);
    assert_memory_equal(r, expected_r, sizeof(r));
    assert_memory_equal(s, expected_s, sizeof(s));
}

static void test_ecsig_to_rs_wider_output(void **state)
{
    uint8_t r[34];
    uint8_t s[34];
    (void)state;

    memset(r, 0xff, sizeof(r));
    memset(s, 0xff, sizeof(s));
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), PB_OK);
    assert_int_equal(r[0], 0x00);
    assert_int_equal(r[1], 0x00);
    assert_int_equal(r[2], 0x81);
    assert_int_equal(r[33], 0x81);
    assert_int_equal(s[1], 0x00);
    assert_int_equal(s[2], 0x82);
}

static void test_ecsig_to_rs_pads_short_values(void **state)
{
    /* r = 0x0102 and s = 0x7f, as an encoder without padding writes them */
    static const uint8_t sig[] = { 0x30, 0x07, 0x02, 0x02, 0x01, 0x02, 0x02, 0x01, 0x7f };
    uint8_t r[32];
    uint8_t s[32];
    uint8_t expected_r[32] = { 0 };
    uint8_t expected_s[32] = { 0 };
    (void)state;

    expected_r[30] = 0x01;
    expected_r[31] = 0x02;
    expected_s[31] = 0x7f;
    memset(r, 0xff, sizeof(r));
    memset(s, 0xff, sizeof(s));

    assert_int_equal(der_ecsig_to_rs(sig, sizeof(sig), r, s, sizeof(r)), PB_OK);
    assert_memory_equal(r, expected_r, sizeof(r));
    assert_memory_equal(s, expected_s, sizeof(s));
}

static void test_ecsig_too_large(void **state)
{
    struct {
        uint8_t r[32];
        uint8_t s[32];
        uint8_t guard[8];
    } out;

    uint8_t guard[8];
    (void)state;

    memset(guard, 0xee, sizeof(guard));

    /* A 33-byte s that has no leading zero to drop */
    ecsig[39] = 0x01;
    memcpy(out.guard, guard, sizeof(guard));
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), out.r, out.s, sizeof(out.r)),
                     -PB_ERR_BUF_TOO_SMALL);
    assert_memory_equal(out.guard, guard, sizeof(guard));
    make_ecsig();

    /* Same for r. The CAAM driver used to copy all 33 bytes (F-057). */
    ecsig[4] = 0x01;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), out.r, out.s, sizeof(out.r)),
                     -PB_ERR_BUF_TOO_SMALL);
}

static void test_ecsig_truncated(void **state)
{
    uint8_t r[32];
    uint8_t s[32];
    (void)state;

    /* Every prefix fails without reading past it (ASan) */
    for (size_t len = 0; len < sizeof(ecsig); len++) {
        uint8_t *sig = malloc(len ? len : 1);
        assert_non_null(sig);
        memcpy(sig, ecsig, len);
        assert_int_not_equal(der_ecsig_to_rs(sig, len, r, s, sizeof(r)), PB_OK);
        free(sig);
    }

    /* A long form length whose bytes are past the end */
    static const uint8_t long_len[] = { 0x30, 0x46, 0x02, 0x84, 0x00 };
    uint8_t *sig = malloc(sizeof(long_len));
    assert_non_null(sig);
    memcpy(sig, long_len, sizeof(long_len));
    assert_int_equal(der_ecsig_to_rs(sig, sizeof(long_len), r, s, sizeof(r)), -PB_ERR_ASN1);
    free(sig);
}

static void test_ecsig_bad_encoding(void **state)
{
    uint8_t r[32];
    uint8_t s[32];
    (void)state;

    ecsig[0] = 0x31;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[1] = 0x85;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[2] = 0x03;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[3] = 0x85;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), -PB_ERR_ASN1);
    make_ecsig();

    /* An r longer than the signature */
    ecsig[3] = 0x7f;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[37] = 0x03;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), -PB_ERR_ASN1);
    make_ecsig();

    ecsig[38] = 0x85;
    assert_int_equal(der_ecsig_to_rs(ecsig, sizeof(ecsig), r, s, sizeof(r)), -PB_ERR_ASN1);
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
    size_t length = make_ec_key(key, oid, oid_len, point_len);

    memset(expected_point, 0xa5, point_len);

    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind), PB_OK);
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

    size_t length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);

    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind),
                     -PB_ERR_BUF_TOO_SMALL);
}

static void test_ec_public_key_unknown_curve(void **state)
{
    static const uint8_t unknown[] = { 0x2b, 0x81, 0x04, 0x00, 0x99 };
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    (void)state;

    size_t length = make_ec_key(key, unknown, sizeof(unknown), 64);

    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind),
                     -PB_ERR_NOT_IMPLEMENTED);
}

static void test_ec_public_key_not_ec(void **state)
{
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    (void)state;

    size_t length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);

    key[8] = 0x00; /* Last byte of the id-ecPublicKey OID */
    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind), -PB_ERR_ASN1);
}

/* crypto.md 4: the key is parsed inside its length. Every prefix of a key
 * fails, and nothing past it is read (ASan) */
static void test_ec_public_key_truncated(void **state)
{
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    size_t length = make_ec_key(key, secp521r1, sizeof(secp521r1), 132);
    (void)state;

    for (size_t len = 0; len < length; len++) {
        uint8_t *prefix = malloc(len ? len : 1);

        assert_non_null(prefix);
        memcpy(prefix, key, len);
        assert_int_equal(der_ec_public_key_data(prefix, len, out, sizeof(out), &kind),
                         -PB_ERR_ASN1);
        free(prefix);
    }
}

/* An OID is compared whole: one of no bytes matched every OID, and a longer
 * one was compared past the end of pb's own (F-066) */
static void test_ec_public_key_oid_lengths(void **state)
{
    static const uint8_t secp256r1_and_more[] = { 0x2a, 0x86, 0x48, 0xce, 0x3d,
                                                  0x03, 0x01, 0x07, 0x00 };
    static const uint8_t secp384r1_and_more[] = { 0x2b, 0x81, 0x04, 0x00, 0x22, 0x00, 0x00, 0x00 };
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    size_t length;
    (void)state;

    /* No curve */
    length = make_ec_key(key, secp256r1, 0, 64);
    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind),
                     -PB_ERR_NOT_IMPLEMENTED);

    length = make_ec_key(key, secp256r1_and_more, sizeof(secp256r1_and_more), 64);
    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind),
                     -PB_ERR_NOT_IMPLEMENTED);

    length = make_ec_key(key, secp384r1_and_more, sizeof(secp384r1_and_more), 96);
    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind),
                     -PB_ERR_NOT_IMPLEMENTED);

    /* No algorithm: the OID's length is at offset 5 */
    length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
    key[5] = 0;
    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind), -PB_ERR_ASN1);
}

/* The BIT STRING has its two leading bytes, and ends inside the key */
static void test_ec_public_key_bit_string(void **state)
{
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    size_t length;
    (void)state;

    /* The BIT STRING's length is at offset 24 of the secp256r1 key */
    for (uint8_t bytes = 0; bytes < 2; bytes++) {
        length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
        assert_int_equal(key[24], 66);
        key[24] = bytes;
        assert_int_equal(der_ec_public_key_data(key, 25 + bytes, out, sizeof(out), &kind),
                         -PB_ERR_ASN1);
    }

    /* A point that runs past the key */
    length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
    key[24] = 67;
    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind), -PB_ERR_ASN1);
}

/* The point is uncompressed, with no unused bits, and of its curve's length.
 * The two bytes before it were skipped unchecked, and its length wasn't
 * compared with the curve's (F-410) */
static void test_ec_public_key_point(void **state)
{
    static const uint8_t forms[] = { 0x00, 0x02, 0x03, 0x06, 0x07 };
    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    size_t length;
    (void)state;

    /* The unused bits are at 25 of the secp256r1 key, the form at 26 */
    length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
    assert_int_equal(key[25], 0x00);
    assert_int_equal(key[26], 0x04);
    key[25] = 0x01;
    assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind), -PB_ERR_ASN1);

    for (size_t i = 0; i < ARRAY_SIZE(forms); i++) {
        length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);
        key[26] = forms[i];
        assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind),
                         -PB_ERR_ASN1);
    }

    /* A compressed point's length, one byte short, one too many, another
     * curve's */
    static const struct {
        const uint8_t *oid;
        size_t oid_len;
        size_t point_len;
    } lengths[] = {
        { secp256r1, sizeof(secp256r1), 32 },  { secp256r1, sizeof(secp256r1), 63 },
        { secp256r1, sizeof(secp256r1), 65 },  { secp256r1, sizeof(secp256r1), 96 },
        { secp384r1, sizeof(secp384r1), 64 },  { secp384r1, sizeof(secp384r1), 97 },
        { secp521r1, sizeof(secp521r1), 131 }, { secp521r1, sizeof(secp521r1), 96 },
    };

    for (size_t i = 0; i < ARRAY_SIZE(lengths); i++) {
        length = make_ec_key(key, lengths[i].oid, lengths[i].oid_len, lengths[i].point_len);
        assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind),
                         -PB_ERR_ASN1);
    }
}

/* The keys that verified before still parse: every key of a keystore is as
 * bpak writes it (bpak generate keystore re-encodes each with Mbed TLS's
 * mbedtls_pk_write_pubkey_der()), as the internal keystore's three are */
static void test_ec_public_key_keystore(void **state)
{
    extern const struct bpak_keystore keystore_pb;

    static const struct {
        enum bpak_key_kind bpak;
        dsa_t dsa;
        size_t coordinate;
    } kinds[] = {
        { BPAK_KEY_PUB_PRIME256v1, DSA_EC_SECP256r1, 32 },
        { BPAK_KEY_PUB_SECP384r1, DSA_EC_SECP384r1, 48 },
        { BPAK_KEY_PUB_SECP521r1, DSA_EC_SECP521r1, 66 },
    };

    uint8_t out[160];
    unsigned int seen = 0;
    (void)state;

    for (int i = 0; i < keystore_pb.no_of_keys; i++) {
        const struct bpak_key *key = keystore_pb.keys[i];
        size_t point = 0;
        dsa_t kind = 0;

        for (size_t k = 0; k < ARRAY_SIZE(kinds); k++) {
            if (kinds[k].bpak == key->kind) {
                assert_int_equal(
                    der_ec_public_key_data(key->data, key->size, out, sizeof(out), &kind), PB_OK);
                assert_int_equal(kind, kinds[k].dsa);
                point = 2 * kinds[k].coordinate;
                seen |= 1u << k;
            }
        }

        /* The BIT STRING ends the key: no unused bits, uncompressed */
        assert_int_not_equal(point, 0);
        assert_int_equal(key->data[key->size - point - 2], 0x00);
        assert_int_equal(key->data[key->size - point - 1], 0x04);
        assert_memory_equal(out, &key->data[key->size - point], point);
    }

    assert_int_equal(seen, 7);
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
        { 3, 0x85, -PB_ERR_ASN1 },  { 4, 0x05, -PB_ERR_ASN1 },  { 5, 0x85, -PB_ERR_ASN1 },
        { 13, 0x05, -PB_ERR_ASN1 }, { 14, 0x85, -PB_ERR_ASN1 }, { 23, 0x04, -PB_ERR_ASN1 },
        { 24, 0x85, -PB_ERR_ASN1 },
    };

    uint8_t key[256];
    uint8_t out[160];
    dsa_t kind;
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(cases); i++) {
        size_t length = make_ec_key(key, secp256r1, sizeof(secp256r1), 64);

        key[cases[i].offset] = cases[i].value;
        assert_int_equal(der_ec_public_key_data(key, length, out, sizeof(out), &kind), cases[i].rc);
    }
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_asn1_size_forms),
        cmocka_unit_test_setup(test_ecsig_to_rs, setup),
        cmocka_unit_test_setup(test_ecsig_to_rs_wider_output, setup),
        cmocka_unit_test_setup(test_ecsig_to_rs_pads_short_values, setup),
        cmocka_unit_test_setup(test_ecsig_too_large, setup),
        cmocka_unit_test_setup(test_ecsig_truncated, setup),
        cmocka_unit_test_setup(test_ecsig_bad_encoding, setup),
        cmocka_unit_test(test_ec_public_key_data),
        cmocka_unit_test(test_ec_public_key_output_too_small),
        cmocka_unit_test(test_ec_public_key_unknown_curve),
        cmocka_unit_test(test_ec_public_key_not_ec),
        cmocka_unit_test(test_ec_public_key_truncated),
        cmocka_unit_test(test_ec_public_key_oid_lengths),
        cmocka_unit_test(test_ec_public_key_bit_string),
        cmocka_unit_test(test_ec_public_key_bad_encoding),
        cmocka_unit_test(test_ec_public_key_point),
        cmocka_unit_test(test_ec_public_key_keystore),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
