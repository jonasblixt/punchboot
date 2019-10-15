/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
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
static struct fsl_caam_jr *d;
static uint32_t __a4k desc[16];
static uint32_t current_hash_kind;
static volatile __a4k __no_bss uint8_t hash_ctx[128];
static __no_bss __a16b uint8_t caam_tmp_buf[256];
static __no_bss __a16b uint8_t caam_ecdsa_key[256];


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
    uint32_t err;

    if (ctx.sg_count)
    {
        err = caam_wait_for_job(d, desc);

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

    return caam_shedule_job_async(d, desc);
}

static uint32_t caam_hash_finalize(uint32_t alg, uint32_t ctx_sz, uint8_t *out)
{
    uint8_t dc = 0;
    uint32_t err;

    err = caam_wait_for_job(d, desc);

    if (err != PB_OK)
        return err;

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | alg |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_FIN;

    desc[dc++] = LD_NOIMM(CLASS_2, REG_CTX, ctx_sz);
    desc[dc++]  = (uint32_t) (uintptr_t) hash_ctx;
    desc[dc++]  = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    desc[dc++]  = 0;
    desc[dc++]  = 0;
    desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, ctx_sz);
    desc[dc++] = (uint32_t) (uintptr_t) out;

    desc[0] |= dc;

    return caam_shedule_job_sync(d, desc);
}

static uint32_t caam_rsa_enc(uint8_t *input,  uint32_t input_sz,
                    uint8_t *output, struct pb_key *k)
{
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

    return caam_shedule_job_sync(d, desc);
}

static uint32_t caam_ecdsa_verify(uint8_t *hash, uint32_t hash_kind,
                                  uint8_t *sig, uint32_t sig_kind,
                                  struct pb_key *k)
{
    uint8_t dc = 0;
    uint32_t sig_len = 0;
    uint32_t hash_len = 0;
    uint8_t caam_sig_type = 0;
    struct pb_ec_key *key =
        (struct pb_ec_key *) k->data;

    memset(caam_tmp_buf, 0, 256);
    memset(caam_ecdsa_key, 0, 256);

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

    switch (sig_kind)
    {
        case PB_SIGN_NIST256p:
            sig_len = 32;
            caam_sig_type = 2;
        break;
        case PB_SIGN_NIST384p:
            sig_len = 48;
            caam_sig_type = 3;
        break;
        case PB_SIGN_NIST521p:
            sig_len = 66;
            caam_sig_type = 4;
        break;
        default:
            LOG_ERR("Unknown signature format");
            return PB_ERR;
    };

    memcpy(caam_ecdsa_key, key->public_key, sig_len*2);

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = (1 << 22) | (caam_sig_type << 7);
    desc[dc++] = (uint32_t)(uintptr_t) caam_ecdsa_key;
    desc[dc++] = (uint32_t)(uintptr_t) hash;
    desc[dc++] = (uint32_t)(uintptr_t) sig;
    desc[dc++] = (uint32_t)(uintptr_t) &sig[sig_len];
    desc[dc++] = (uint32_t)(uintptr_t) caam_tmp_buf;
    desc[dc++] = hash_len;
    desc[dc++] = CAAM_CMD_OP | (0x16 << 16) | (2 << 10) | (1 << 1);

    desc[0] |= ((dc-1) << 16) | dc;

    return caam_shedule_job_sync(d, desc);
}

uint32_t caam_init(struct fsl_caam_jr *caam_dev)
{
    d = caam_dev;

    if (d->base == 0)
        return PB_ERR;

    for (uint32_t n = 0; n < JOB_RING_ENTRIES; n++)
        d->input_ring[n] = 0;

    for (uint32_t n = 0; n < JOB_RING_ENTRIES*2; n++)
        d->output_ring[n] = 0;

    /* Initialize job rings */
    pb_write32( (uint32_t)(uintptr_t) d->input_ring,  d->base + CAAM_IRBAR);
    pb_write32( (uint32_t)(uintptr_t) d->output_ring, d->base + CAAM_ORBAR);

    pb_write32(JOB_RING_ENTRIES, d->base + CAAM_IRSR);
    pb_write32(JOB_RING_ENTRIES, d->base + CAAM_ORSR);

    return PB_OK;
}


/* Crypto Interface */
uint32_t  plat_hash_init(uint32_t hash_kind)
{
    uint32_t err = PB_ERR;

    current_hash_kind = hash_kind;
    switch (hash_kind)
    {
        case PB_HASH_MD5:
            err = caam_hash_init(CAAM_ALG_TYPE_MD5);
        break;
        case PB_HASH_SHA256:
            err = caam_hash_init(CAAM_ALG_TYPE_SHA256);
        break;
        case PB_HASH_SHA384:
            err = caam_hash_init(CAAM_ALG_TYPE_SHA384);
        break;
        case PB_HASH_SHA512:
            err = caam_hash_init(CAAM_ALG_TYPE_SHA512);
        break;
        default:
            LOG_ERR("Unknown hash");
            current_hash_kind = PB_HASH_INVALID;
            err = PB_ERR;
        break;
    }

    return err;
}

uint32_t  plat_hash_update(uintptr_t bfr, uint32_t sz)
{
    uint32_t err;

    switch (current_hash_kind)
    {
        case PB_HASH_SHA256:
            err = caam_hash_update(CAAM_ALG_TYPE_SHA256, 64, (uint8_t *)bfr,
                                                                sz);
        break;
        case PB_HASH_SHA384:
            err = caam_hash_update(CAAM_ALG_TYPE_SHA384, 96, (uint8_t *)bfr,
                                                                sz);
        break;
        case PB_HASH_SHA512:
            err = caam_hash_update(CAAM_ALG_TYPE_SHA512, 128, (uint8_t *)bfr,
                                                                sz);
        break;
        case PB_HASH_MD5:
            err = caam_hash_update(CAAM_ALG_TYPE_MD5, 16, (uint8_t *)bfr, sz);
        break;
        default:
            err = PB_ERR;
    }

    return err;
}

uint32_t  plat_hash_finalize(uintptr_t out)
{
    uint32_t err;

    switch (current_hash_kind)
    {
        case PB_HASH_SHA256:
        {
            err = caam_hash_finalize(CAAM_ALG_TYPE_SHA256, 64, (uint8_t *)out);
        }
        break;
        case PB_HASH_SHA384:
        {
            err = caam_hash_finalize(CAAM_ALG_TYPE_SHA384, 96, (uint8_t *)out);
        }
        break;
        case PB_HASH_SHA512:
        {
            err = caam_hash_finalize(CAAM_ALG_TYPE_SHA512, 128, (uint8_t *)out);
        }
        break;
        case PB_HASH_MD5:
        {
            err = caam_hash_finalize(CAAM_ALG_TYPE_MD5, 16, (uint8_t *)out);
        }
        break;
        default:
            err = PB_ERR;
    }

    return err;
}

static char __no_bss __a4k output_data[1024];

uint32_t  plat_verify_signature(uint8_t *sig, uint32_t sig_kind,
                                uint8_t *hash, uint32_t hash_kind,
                                struct pb_key *k)
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
        case PB_SIGN_NIST521p:
        {
            LOG_DBG("Checking EC521 signature...");
            err = caam_ecdsa_verify(hash, hash_kind, sig, PB_SIGN_NIST521p, k);
        }
        break;
        case PB_SIGN_NIST384p:
        {
            LOG_DBG("Checking EC384 signature...");
            err = caam_ecdsa_verify(hash, hash_kind, sig, PB_SIGN_NIST384p, k);
        }
        break;
        case PB_SIGN_NIST256p:
        {
            LOG_DBG("Checking EC256 signature...");
            err = caam_ecdsa_verify(hash, hash_kind, sig, PB_SIGN_NIST256p, k);
        }
        break;
        default:
            LOG_ERR("Unknown signature format");
            err = PB_ERR;
        break;
    }

    return err;
}
