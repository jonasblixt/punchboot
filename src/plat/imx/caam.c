/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <plat.h>
#include <io.h>
#include <pb.h>
#include <string.h>
#include <crypto.h>
#include <plat/imx/caam.h>
#include <plat/imx/desc_defines.h>
#include <plat/imx/desc_helper.h>
#include <bpak/keystore.h>

/* Commands  */
#define CAAM_CMD_HEADER  0xB0800000
#define CAAM_CMD_OP      0x80000000
#define CAAM_CMD_LOAD    0x10000000
#define CAAM_CMD_STORE   0x50000000
#define CAAM_CMD_SEQIN   0xF0000000
#define CAAM_CMD_SEQOUT  0xF8000000
#define CAAM_CMD_FIFOL   0x20000000

/* Operations */
#define CAAM_OP_ALG_CLASS1      (0x02 << 24)
#define CAAM_OP_ALG_CLASS2      (0x04 << 24)

/* Alg operation defines */

#define CAAM_ALG_TYPE_SHA512    (0x45 << 16)
#define CAAM_ALG_TYPE_SHA384    (0x44 << 16)
#define CAAM_ALG_TYPE_SHA256    (0x43 << 16)
#define CAAM_ALG_TYPE_MD5       (0x40 << 16)
#define CAAM_ALG_STATE_UPDATE   (0x00 << 2)
#define CAAM_ALG_STATE_INIT     (0x01 << 2)
#define CAAM_ALG_STATE_FIN      (0x02 << 2)
#define CAAM_ALG_STATE_INIT_FIN (0x03 << 2)
#define CAAM_ALG_AAI(x)         (x << 4)

static __no_bss struct caam_hash_ctx ctx;
static struct fsl_caam_jr *dev;
static uint32_t __a4k desc[16];
static uint32_t current_hash_kind;
static uint32_t hash_tmp_buf_count;
static volatile __a4k __no_bss uint8_t hash_ctx[128];
static __no_bss __a4k uint8_t caam_tmp_buf[256];
static __no_bss __a4k uint8_t caam_ecdsa_key[256];
static __no_bss __a4k uint8_t caam_ecdsa_r[66];
static __no_bss __a4k uint8_t caam_ecdsa_s[66];
static __no_bss __a4k uint8_t caam_ecdsa_hash[128];
static char __no_bss __a4k output_data[1024];

static uint32_t caam_shedule_job_async(struct fsl_caam_jr *d, uint32_t *job)
{
    if (d == NULL)
        return PB_ERR;

    d->input_ring[0] = (uint32_t)(uintptr_t) job;


    pb_write32(1, d->base + CAAM_IRJAR);

    return PB_OK;
}

static uint32_t caam_shedule_job_sync(struct fsl_caam_jr *d, uint32_t *job)
{
    uint32_t err;

    err = caam_shedule_job_async(d, job);

    if (err != PB_OK)
        return err;

    while ((pb_read32(d->base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    if (d->output_ring[0] != (uint32_t)(uintptr_t) job)
    {
        LOG_ERR("Job failed");
        return PB_ERR;
    }
    pb_write32(1, d->base + CAAM_ORJRR);

    uint32_t caam_status = pb_read32(d->base + CAAM_JRSTAR);

    if (caam_status)
    {
        LOG_ERR("Job error %08x", caam_status);
        return PB_ERR;
    }
    return PB_OK;
}



static uint32_t caam_wait_for_job(struct fsl_caam_jr *d, uint32_t *job)
{
    while ((pb_read32(d->base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    if (d->output_ring[0] != (uint32_t)(uintptr_t) job)
    {
        LOG_ERR("Job failed\n\r");
        return PB_ERR;
    }
    pb_write32(1, d->base + CAAM_ORJRR);

    uint32_t caam_status = pb_read32(d->base + CAAM_JRSTAR);

    if (caam_status)
    {
        LOG_ERR("Job error %08x", caam_status);
        return PB_ERR;
    }
    return PB_OK;
}

static uint32_t caam_hash_init(uint32_t alg)

{
    UNUSED(alg);
    memset((void *)hash_ctx, 0, 128);
    memset(&ctx, 0, sizeof(struct caam_hash_ctx));
    return PB_OK;
}

static uint32_t caam_hash_update(uint32_t alg, uint32_t ctx_sz,
                                uint8_t *data, uint32_t sz)
{
    uint8_t dc = 0;
    uint32_t err = PB_OK;

    if (ctx.sg_count)
    {
        err = caam_wait_for_job(dev, desc);

        if (err != PB_OK)
            return err;
    }

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | alg |
        CAAM_ALG_AAI(0);


    if (ctx.sg_count == 0)
    {
        desc[1] |= CAAM_ALG_STATE_INIT;
        LOG_INFO("Init %p, %u", data, sz);
    }
    else
    {
        desc[1] |= CAAM_ALG_STATE_UPDATE;
        desc[dc++]  = LD_NOIMM(CLASS_2, REG_CTX, ctx_sz);
        desc[dc++]  = (uint32_t) (uintptr_t) hash_ctx;
        LOG_INFO("Update %p, %u", data, sz);
    }

    ctx.sg_count = 1;

    desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    desc[dc++] = (uint32_t) (uintptr_t) data;
    desc[dc++] = sz;
    desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, ctx_sz);
    desc[dc++] = (uint32_t) (uintptr_t) hash_ctx;

    desc[0] |= dc;

    return caam_shedule_job_async(dev, desc);

}


static uint32_t caam_hash_finalize(uint32_t alg, uint32_t ctx_sz,
                                   uint8_t *data, uint32_t sz,  uint8_t *out)
{
    uint8_t dc = 0;
    uint32_t err;

    err = caam_wait_for_job(dev, desc);

    if (err != PB_OK)
        return err;

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | alg |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_FIN;

    desc[dc++] = LD_NOIMM(CLASS_2, REG_CTX, ctx_sz);
    desc[dc++]  = (uint32_t) (uintptr_t) hash_ctx;
    desc[dc++]  = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    desc[dc++]  = (uint32_t) (uintptr_t) data;
    desc[dc++]  = sz;
    desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, ctx_sz);
    desc[dc++] = (uint32_t) (uintptr_t) out;

    desc[0] |= dc;

    return caam_shedule_job_sync(dev, desc);
}

static uint32_t caam_rsa_enc(uint8_t *input,  uint32_t input_sz,
                    uint8_t *output, struct bpak_key *k)
{
/*
    struct pb_rsa4096_key *rsa_key =
        (struct pb_rsa4096_key *) k->data;

    desc[0] = CAAM_CMD_HEADER | (7 << 16) | 8;
    desc[1] = (3 << 12)|512;
    desc[2] = (uint32_t)(uintptr_t) input;
    desc[3] = (uint32_t)(uintptr_t) output;
    desc[4] = (uint32_t)(uintptr_t) rsa_key->mod;
    desc[5] = (uint32_t)(uintptr_t) rsa_key->exp;
    desc[6] = input_sz;
    desc[7] = CAAM_CMD_OP | (0x18 << 16);

    return caam_shedule_job_sync(dev, desc);
 */
    return PB_ERR;
}

static uint32_t caam_ecdsa_verify(uint8_t *hash, uint32_t hash_kind,
                                  uint8_t *sig, uint32_t sig_kind,
                                  struct bpak_key *k)
{
    uint8_t dc = 0;
    uint32_t hash_len = 0;
    uint8_t caam_sig_type = 0;
    uint8_t *key_data = NULL;
    uint8_t key_sz = 0;
    uint8_t *r = NULL;
    uint8_t *s = NULL;

    if (pb_asn1_eckey_data(k, &key_data, &key_sz) != PB_OK)
    {
        LOG_ERR("Could not extract key data");
        return PB_ERR;
    }

    if (pb_asn1_ecsig_to_rs(sig, sig_kind, &r, &s) != PB_OK)
    {
        LOG_ERR("Could not get r/s values");
        return PB_ERR;
    }

    memset(caam_tmp_buf, 0, 256);
    memset(caam_ecdsa_key, 0, 256);
    memcpy(caam_ecdsa_key, key_data, key_sz);
    memcpy(caam_ecdsa_r, r, 66);
    memcpy(caam_ecdsa_s, s, 66);

    switch (hash_kind)
    {
        case PB_HASH_SHA256:
            hash_len = 32;
        break;
        case PB_HASH_SHA384:
            hash_len = 48;
        break;
        case PB_HASH_SHA512:
            hash_len = 64;
        break;
        default:
            LOG_ERR("Unknown hash");
            return PB_ERR;
    };

    memcpy(caam_ecdsa_hash, hash, hash_len);

    switch (sig_kind)
    {
        case PB_SIGN_PRIME256v1:
            caam_sig_type = 2;
        break;
        case PB_SIGN_SECP384r1:
            caam_sig_type = 3;
        break;
        case PB_SIGN_SECP521r1:
            caam_sig_type = 4;
        break;
        default:
            LOG_ERR("Unknown signature format");
            return PB_ERR;
    };

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = (1 << 22) | (caam_sig_type << 7);
    desc[dc++] = (uint32_t)(uintptr_t) caam_ecdsa_key;
    desc[dc++] = (uint32_t)(uintptr_t) caam_ecdsa_hash;
    desc[dc++] = (uint32_t)(uintptr_t) caam_ecdsa_r;
    desc[dc++] = (uint32_t)(uintptr_t) caam_ecdsa_s;
    desc[dc++] = (uint32_t)(uintptr_t) caam_tmp_buf;
    desc[dc++] = hash_len;
    desc[dc++] = CAAM_CMD_OP | (0x16 << 16) | (2 << 10) | (1 << 1);

    desc[0] |= ((dc-1) << 16) | dc;

    return caam_shedule_job_sync(dev, desc);
}

uint32_t caam_init(struct fsl_caam_jr *caam_dev)
{
    dev = caam_dev;

    if (dev->base == 0)
        return PB_ERR;

    for (uint32_t n = 0; n < JOB_RING_ENTRIES; n++)
        dev->input_ring[n] = 0;

    for (uint32_t n = 0; n < JOB_RING_ENTRIES*2; n++)
        dev->output_ring[n] = 0;

    /* Initialize job rings */
    pb_write32( (uint32_t)(uintptr_t) dev->input_ring,  dev->base + CAAM_IRBAR);
    pb_write32( (uint32_t)(uintptr_t) dev->output_ring, dev->base + CAAM_ORBAR);

    pb_write32(JOB_RING_ENTRIES, dev->base + CAAM_IRSR);
    pb_write32(JOB_RING_ENTRIES, dev->base + CAAM_ORSR);

    return PB_OK;
}


/* Crypto Interface */
uint32_t  plat_hash_init(uint32_t hash_kind)
{
    uint32_t alg = 0;

    current_hash_kind = hash_kind;
    hash_tmp_buf_count = 0;

    switch (current_hash_kind)
    {
        case PB_HASH_SHA256:
            alg = CAAM_ALG_TYPE_SHA256;
        break;
        case PB_HASH_SHA384:
            alg = CAAM_ALG_TYPE_SHA384;
        break;
        case PB_HASH_SHA512:
            alg = CAAM_ALG_TYPE_SHA512;
        break;
        case PB_HASH_MD5:
            alg = CAAM_ALG_TYPE_MD5;
        break;
        default:
            return PB_ERR;
    }

    return caam_hash_init(alg);
}

uint32_t  plat_hash_update(uintptr_t bfr, uint32_t sz)
{
    uint32_t alg = 0;
    uint32_t block_size = 0;

    switch (current_hash_kind)
    {
        case PB_HASH_SHA256:
            alg = CAAM_ALG_TYPE_SHA256;
            block_size = 64;
        break;
        case PB_HASH_SHA384:
            alg = CAAM_ALG_TYPE_SHA384;
            block_size = 96;
        break;
        case PB_HASH_SHA512:
            alg = CAAM_ALG_TYPE_SHA512;
            block_size = 128;
        break;
        case PB_HASH_MD5:
            alg = CAAM_ALG_TYPE_MD5;
            block_size = 16;
        break;
        default:
            return PB_ERR;
    }

    return caam_hash_update(alg, block_size, (uint8_t *) bfr, sz);
}

uint32_t  plat_hash_finalize(uintptr_t data, uint32_t sz, uintptr_t out,
                                uint32_t out_sz)
{
    uint32_t err;
    uint32_t alg = 0;
    uint32_t block_size = 0;

    switch (current_hash_kind)
    {
        case PB_HASH_SHA256:
            alg = CAAM_ALG_TYPE_SHA256;
            block_size = 64;
            if (out_sz < 32)
                return PB_ERR;
        break;
        case PB_HASH_SHA384:
            alg = CAAM_ALG_TYPE_SHA384;
            block_size = 96;

            if (out_sz < 48)
                return PB_ERR;
        break;
        case PB_HASH_SHA512:
            alg = CAAM_ALG_TYPE_SHA512;
            block_size = 128;

            if (out_sz < 64)
                return PB_ERR;
        break;
        case PB_HASH_MD5:
            alg = CAAM_ALG_TYPE_MD5;
            block_size = 16;

            if (out_sz < 16)
                return PB_ERR;
        break;
        default:
            return PB_ERR;
    }

    err = caam_hash_finalize(alg, block_size, (uint8_t *) data, sz, (uint8_t *)out);
    return err;
}


uint32_t  plat_verify_signature(uint8_t *sig, uint32_t sig_kind,
                                uint8_t *hash, uint32_t hash_kind,
                                struct bpak_key *k)
{
    uint32_t err = PB_ERR;
    uint8_t hash_length;

    switch (hash_kind)
    {
        case PB_HASH_SHA256:
            hash_length = 32;
        break;
        case PB_HASH_SHA384:
            hash_length = 48;
        break;
        case PB_HASH_SHA512:
            hash_length = 64;
        break;
        default:
            return PB_ERR;
    }


    switch (sig_kind)
    {
        case PB_SIGN_RSA4096:
        {
            LOG_DBG("Checking RSA4096 signature...");
            err = caam_rsa_enc(sig, 512,
                            (uint8_t *) output_data, k);


            if (err != PB_OK)
                break;

            if (memcmp(&output_data[512-hash_length], hash, hash_length) == 0)
                err = PB_OK;
            else
                err = PB_ERR;

            LOG_DBG("Signature %s", (err == PB_OK)?"OK":"Fail");
        }
        break;
        case PB_SIGN_PRIME256v1:
        case PB_SIGN_SECP384r1:
        case PB_SIGN_SECP521r1:
            LOG_DBG("Checking EC signature...");
            err = caam_ecdsa_verify(hash, hash_kind, sig, sig_kind, k);
        break;
        default:
            LOG_ERR("Unknown signature format");
            err = PB_ERR;
        break;
    }

    return err;
}
