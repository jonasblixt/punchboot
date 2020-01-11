/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/crypto.h>
#include <bpak/keystore.h>

static const char ec_identifier[] = "\x2a\x86\x48\xce\x3d\x02\x01";
static const char rsa_identifier[] = "\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01";

int pb_asn1_size(const uint8_t *buf, size_t *sz)
{
    uint8_t n_octets = 1;
    size_t result = 0;
    uint8_t r[4];

    if (buf[0] & 0x80)
    {
        n_octets = buf[0] & 0x7f;

        for (int i = 0; i < n_octets; i++)
        {
            r[n_octets-i-1] = buf[i+1];
        }

        result = *((uint16_t *) r);
        n_octets++;
    }
    else
    {
        result = buf[0];
    }

    *sz = result;
    return n_octets;
}

int pb_asn1_eckey_data(struct bpak_key *k, uint8_t **data, uint8_t *key_sz)
{
    size_t s;
    int n;
    uint8_t *p = k->data;

    if (*p++ != 0x30)
        return PB_ERR;

    n = pb_asn1_size(p, &s);
    p += n;

    if (*p++ != 0x30)
        return PB_ERR;

    n = pb_asn1_size(p, &s);
    p += n;

    if (*p++ != 0x06)
        return PB_ERR;

    n = pb_asn1_size(p, &s);
    p += n;

    if (memcmp(p, ec_identifier, s) == 0)
    {
        p += s;

        if (*p++ != 0x06)
            return PB_ERR;

        n = pb_asn1_size(p, &s);
        p += n;
        p += s;

        if (*p++ != 0x03)
            return PB_ERR;

        n = pb_asn1_size(p, &s);
        p += n;

        p += 2; /* Skip unused bits and compression point */
        s -= 2;

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
    int n;
    uint8_t *p = k->data;

    if (*p++ != 0x30)
        return PB_ERR;

    n = pb_asn1_size(p, &s);
    p += n;

    if (*p++ != 0x30)
        return PB_ERR;

    n = pb_asn1_size(p, &s);
    p += n;

    if (*p++ != 0x06)
        return PB_ERR;

    n = pb_asn1_size(p, &s);
    p += n;

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
        n = pb_asn1_size(p, &s);
        p += n;
        p++;

        if (*p++ != 0x30)
        {
            return PB_ERR;
        }

        n = pb_asn1_size(p, &s);
        p += n;

        if (*p++ != 0x02)
        {
            return PB_ERR;
        }
        n = pb_asn1_size(p, &s);
        p += n;

        *mod = p+1;
        p += s;

        if (*p++ != 0x02)
        {
            return PB_ERR;
        }
        n = pb_asn1_size(p, &s);
        p += n;
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
    int n;
    uint8_t *data_r = *r;
    uint8_t *data_s = *s;
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

    n = pb_asn1_size(p, &sz);
    p += n;

    if (*p++ != 0x02)
        return PB_ERR;

    n = pb_asn1_size(p, &sz);
    p += n;
    offset = sz - rs_length;
    //memset(data_r, 0, offset);
    *r = p + offset;
    p += sz;

    if (*p++ != 0x02)
        return PB_ERR;

    n = pb_asn1_size(p, &sz);
    p += n;
    offset = sz - rs_length;
    //memset(data_s, 0, offset);

    *s = p + offset;
    return PB_OK;
}
