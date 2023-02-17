/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/asn1.h>
#include <bpak/keystore.h>

static const char ec_identifier[] = "\x2a\x86\x48\xce\x3d\x02\x01";
static const char rsa_identifier[] = "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01";
static uint8_t sig_ec_r[128] __no_bss;
static uint8_t sig_ec_s[128] __no_bss;

/* Copied from mbedtls */
int pb_asn1_size(unsigned char **p, size_t *len)
{

    if (( **p & 0x80) == 0)
        *len = *(*p)++;
    else
    {
        switch( **p & 0x7F )
        {
        case 1:
            *len = (*p)[1];
            (*p) += 2;
            break;
        case 2:
            *len = ( (size_t)(*p)[1] << 8 ) | (*p)[2];
            (*p) += 3;
            break;
        case 3:
            *len = ( (size_t)(*p)[1] << 16 ) |
                   ( (size_t)(*p)[2] << 8  ) | (*p)[3];
            (*p) += 4;
            break;
        case 4:
            *len = ( (size_t)(*p)[1] << 24 ) | ( (size_t)(*p)[2] << 16 ) |
                   ( (size_t)(*p)[3] << 8  ) |           (*p)[4];
            (*p) += 5;
            break;
        default:
            return -PB_ERR;
        }
    }

    return PB_OK;
}

int pb_asn1_eckey_data(struct bpak_key *k, uint8_t **data, size_t *key_sz,
                        bool include_compression_point)
{
    size_t s;
    int rc;
    uint8_t *p = k->data;

    if (*p++ != 0x30)
        return PB_ERR;

    rc = pb_asn1_size(&p, &s);

    if (rc != PB_OK)
        return rc;

    if (*p++ != 0x30)
        return PB_ERR;

    rc = pb_asn1_size(&p, &s);

    if (rc != PB_OK)
        return rc;

    if (*p++ != 0x06)
        return PB_ERR;

    rc = pb_asn1_size(&p, &s);

    if (rc != PB_OK)
        return rc;

    if (memcmp(p, ec_identifier, s) == 0)
    {
        p += s;

        if (*p++ != 0x06)
            return PB_ERR;

        rc = pb_asn1_size(&p, &s);

        if (rc != PB_OK)
            return rc;

        p += s;

        if (*p++ != 0x03)
            return PB_ERR;

        rc = pb_asn1_size(&p, &s);

        if (rc != PB_OK)
            return rc;

        if (include_compression_point)
        {
            p += 1; /* Skip unused bits */
            s -= 1;
        }
        else
        {
            p += 2; /* Skip unused bits and compression point */
            s -= 2;
        }

        (*data) = p;
        (*key_sz) = s;

    }
    else
    {
        return PB_ERR;
    }

    return PB_OK;
}


int pb_asn1_rsa_data(struct bpak_key *k, uint8_t **mod, uint8_t **exp)
{
    size_t s;
    int rc;
    uint8_t *p = k->data;

    if (*p++ != 0x30)
        return PB_ERR;

    rc = pb_asn1_size(&p, &s);

    if (rc != PB_OK)
        return rc;

    if (*p++ != 0x30)
        return PB_ERR;

    rc = pb_asn1_size(&p, &s);

    if (rc != PB_OK)
        return rc;

    if (*p++ != 0x06)
        return PB_ERR;

    rc = pb_asn1_size(&p, &s);

    if (rc != PB_OK)
        return rc;

    if (memcmp(p, rsa_identifier, s) == 0)
    {
        p += s;

        if (*p++ != 0x05) /* NULL */
        {
            return PB_ERR;
        }
        p++;

        if (*p++ != 0x03)
        {
            return PB_ERR;
        }
        rc = pb_asn1_size(&p, &s);

        if (rc != PB_OK)
            return rc;
        p++;

        if (*p++ != 0x30)
        {
            return PB_ERR;
        }

        rc = pb_asn1_size(&p, &s);

        if (rc != PB_OK)
            return rc;

        if (*p++ != 0x02)
        {
            return PB_ERR;
        }
        rc = pb_asn1_size(&p, &s);

        if (rc != PB_OK)
            return rc;

        *mod = p+1;
        p += s;

        if (*p++ != 0x02)
        {
            return PB_ERR;
        }
        rc = pb_asn1_size(&p, &s);

        if (rc != PB_OK)
            return rc;
        *exp = p;
    }
    else
    {
        return PB_ERR;
    }

    return PB_OK;
}

int pb_asn1_ecsig_to_rs(uint8_t *sig, uint8_t sig_kind,
                            uint8_t **r, uint8_t **s)
{
    size_t sz;
    int rc;
    uint8_t offset;
    uint8_t rs_length;
    uint8_t *p = sig;

    switch (sig_kind)
    {
        case BPAK_SIGN_PRIME256v1:
            rs_length = 32;
        break;
        case BPAK_SIGN_SECP384r1:
            rs_length = 48;
        break;
        case BPAK_SIGN_SECP521r1:
            rs_length = 66;
        break;
        default:
            LOG_ERR("Unkown signature kind");
            return PB_ERR;
    }

    if (*p++ != 0x30)
    {
        LOG_ERR("Missing seq");
        return PB_ERR;
    }

    rc = pb_asn1_size(&p, &sz);

    if (rc != PB_OK)
        return rc;

    if (*p++ != 0x02)
        return PB_ERR;


    memset(sig_ec_r, 0, sizeof(sig_ec_r));
    memset(sig_ec_s, 0, sizeof(sig_ec_s));

    rc = pb_asn1_size(&p, &sz);

    if (rc != PB_OK)
        return rc;

    if (sz > rs_length)
    {
        p++;
        sz--;
    }
    offset = rs_length - sz;
    *r = sig_ec_r;

    memcpy(sig_ec_r + offset, p, sz);
    p += sz;

    if (*p++ != 0x02)
        return PB_ERR;

    rc = pb_asn1_size(&p, &sz);

    if (rc != PB_OK)
        return rc;

    if (sz > rs_length)
    {
        p++;
        sz--;
    }
    offset = rs_length - sz;
    *s = sig_ec_s;

    memcpy(sig_ec_s + offset, p, sz);
    return PB_OK;
}
