/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/crypto.h>
#include <pb/der_helpers.h>
#include <pb/errors.h>
#include <stdio.h>
#include <string.h>

static const char ec_identifier[] = "\x2a\x86\x48\xce\x3d\x02\x01";
static const uint8_t secp256r1_oid[] = "\x2a\x86\x48\xce\x3d\x03\x01\x07";
static const uint8_t secp384r1_oid[] = "\x2b\x81\x04\x00\x22";
static const uint8_t secp521r1_oid[] = "\x2b\x81\x04\x00\x23";

/* Copied from mbedtls */
int asn1_size(const unsigned char **p, size_t *len)
{
    if ((**p & 0x80) == 0) {
        *len = *(*p)++;
    } else {
        switch (**p & 0x7F) {
        case 1:
            *len = (*p)[1];
            (*p) += 2;
            break;
        case 2:
            *len = ((size_t)(*p)[1] << 8) | (*p)[2];
            (*p) += 3;
            break;
        case 3:
            *len = ((size_t)(*p)[1] << 16) | ((size_t)(*p)[2] << 8) | (*p)[3];
            (*p) += 4;
            break;
        case 4:
            *len = ((size_t)(*p)[1] << 24) | ((size_t)(*p)[2] << 16) | ((size_t)(*p)[3] << 8) |
                   (*p)[4];
            (*p) += 5;
            break;
        default:
            return -PB_ERR_ASN1;
        }
    }

    return PB_OK;
}

/* Checks that the length bytes of a DER header at p fit before end */
static int asn1_length_fits(const uint8_t *p, const uint8_t *end)
{
    if (p >= end)
        return -PB_ERR_ASN1;

    if ((*p & 0x80) && (size_t)(end - p) < 1u + (*p & 0x7f))
        return -PB_ERR_ASN1;

    return PB_OK;
}

/* Reads an INTEGER as a big-endian value of 'length' bytes, zero padded */
static int der_integer_to_fixed(const uint8_t **p, const uint8_t *end, uint8_t *out, size_t length)
{
    size_t sz;
    int rc;

    if (*p >= end || *(*p)++ != 0x02)
        return -PB_ERR_ASN1;

    rc = asn1_length_fits(*p, end);

    if (rc != PB_OK)
        return rc;

    rc = asn1_size(p, &sz);

    if (rc != PB_OK)
        return rc;

    if (sz > (size_t)(end - *p))
        return -PB_ERR_ASN1;

    const uint8_t *value = *p;
    *p += sz;

    /* EC r/s values are positive, so DER adds a leading zero when the
     * MSB is set */
    while (sz > length && *value == 0) {
        value++;
        sz--;
    }

    if (sz > length)
        return -PB_ERR_BUF_TOO_SMALL;

    memset(out, 0, length - sz);
    memcpy(&out[length - sz], value, sz);
    return PB_OK;
}

int der_ecsig_to_rs(const uint8_t *sig, size_t sig_length, uint8_t *r, uint8_t *s, size_t length)
{
    size_t sz;
    int rc;
    const uint8_t *p = sig;
    const uint8_t *end = sig + sig_length;

    if (sig_length == 0 || *p++ != 0x30)
        return -PB_ERR_ASN1;

    rc = asn1_length_fits(p, end);

    if (rc != PB_OK)
        return rc;

    /* The SEQUENCE length isn't checked against the integers, because
     * earlier releases ignored it (D-008). Both are bounded by sig_length. */
    rc = asn1_size(&p, &sz);

    if (rc != PB_OK)
        return rc;

    rc = der_integer_to_fixed(&p, end, r, length);

    if (rc != PB_OK)
        return rc;

    return der_integer_to_fixed(&p, end, s, length);
}

int der_ec_public_key_data(const uint8_t *pub_key_der,
                           uint8_t *output,
                           size_t output_length,
                           dsa_t *key_kind)
{
    size_t s;
    int rc;
    const uint8_t *p = pub_key_der;

    if (*p++ != 0x30)
        return -PB_ERR_ASN1;

    rc = asn1_size(&p, &s);

    if (rc != 0)
        return rc;

    if (*p++ != 0x30)
        return -PB_ERR_ASN1;

    rc = asn1_size(&p, &s);

    if (rc != 0)
        return rc;

    if (*p++ != 0x06)
        return -1;

    rc = asn1_size(&p, &s);

    if (rc != 0)
        return rc;

    if (memcmp(p, ec_identifier, s) == 0) {
        p += s;

        if (*p++ != 0x06)
            return -1;

        rc = asn1_size(&p, &s);

        if (rc != 0)
            return rc;

        if (memcmp(p, secp256r1_oid, s) == 0)
            *key_kind = DSA_EC_SECP256r1;
        else if (memcmp(p, secp384r1_oid, s) == 0)
            *key_kind = DSA_EC_SECP384r1;
        else if (memcmp(p, secp521r1_oid, s) == 0)
            *key_kind = DSA_EC_SECP521r1;
        else
            return -PB_ERR_NOT_IMPLEMENTED;

        p += s;

        if (*p++ != 0x03)
            return -1;

        rc = asn1_size(&p, &s);

        if (rc != 0)
            return rc;

        p += 2; /* Skip unused bits and compression point */
        s -= 2;

        if (s > output_length)
            return -PB_ERR_BUF_TOO_SMALL;
        memcpy(output, p, s);
    } else {
        return -PB_ERR_ASN1;
    }

    return PB_OK;
}
