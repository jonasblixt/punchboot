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
#include <pb/plat.h>
#include <pb/io.h>
#include <pb/crypto.h>
#include <pb/asn1.h>
#include <bpak/keystore.h>
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

static uint8_t block_size;
static uint32_t desc[16] __a4k __no_bss;
static uint8_t caam_tmp_buf[256] __a4k __no_bss;
static uint8_t caam_ecdsa_key[256] __a4k __no_bss;
static uint8_t caam_ecdsa_r[66] __a4k __no_bss;
static uint8_t caam_ecdsa_s[66] __a4k __no_bss;
static uint8_t caam_ecdsa_hash[128] __a4k __no_bss;
static char output_data[1024] __a4k __no_bss;
static uint32_t input_ring[JOB_RING_ENTRIES] __a4k __no_bss;
static uint32_t output_ring[JOB_RING_ENTRIES*2] __a4k __no_bss;
static uint32_t alg;

static int caam_shedule_job_async(uint32_t *job)
{
    input_ring[0] = (uint32_t)(uintptr_t) job;
    arch_clean_cache_range((uintptr_t) input_ring, sizeof(input_ring[0]));
    pb_write32(1, CONFIG_IMX_CAAM_BASE + CAAM_IRJAR);

    return PB_OK;
}

static int caam_shedule_job_sync(uint32_t *job)
{
    int err;

    err = caam_shedule_job_async(job);

    if (err != PB_OK)
        return err;

    while ((pb_read32(CONFIG_IMX_CAAM_BASE + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    arch_invalidate_cache_range((uintptr_t) output_ring, sizeof(output_ring[0]));
    if (output_ring[0] != (uint32_t)(uintptr_t) job)
    {
        LOG_ERR("Job failed");
        return -PB_ERR;
    }

    pb_write32(1, CONFIG_IMX_CAAM_BASE + CAAM_ORJRR);

    uint32_t caam_status = pb_read32(CONFIG_IMX_CAAM_BASE + CAAM_JRSTAR);

    if (caam_status)
    {
        LOG_ERR("Job error %08x", caam_status);
        return -PB_ERR;
    }

    return PB_OK;
}

static int caam_wait_for_job(uint32_t *job)
{

    while ((pb_read32(CONFIG_IMX_CAAM_BASE + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    arch_invalidate_cache_range((uintptr_t) output_ring, sizeof(output_ring[0]));

    if (output_ring[0] != (uint32_t)(uintptr_t) job)
    {
        LOG_ERR("Job failed\n\r");
        return -PB_ERR;
    }

    pb_write32(1, CONFIG_IMX_CAAM_BASE + CAAM_ORJRR);

    uint32_t caam_status = pb_read32(CONFIG_IMX_CAAM_BASE + CAAM_JRSTAR);

    if (caam_status)
    {
        LOG_ERR("Job error %08x %p", caam_status, job);
        return -PB_ERR;
    }

    return PB_OK;
}

static int caam_rsa_enc(uint8_t *signature, size_t input_sz,
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

static int caam_ecdsa_verify(struct pb_hash_context *hash,
                            struct bpak_key *key,
                            void *signature)
{
    int dc = 0;
    int hash_len = 0;
    int caam_sig_type = 0;
    uint8_t *key_data = NULL;
    size_t key_sz = 0;
    uint8_t *r = NULL;
    uint8_t *s = NULL;

    if (pb_asn1_eckey_data(key, &key_data, &key_sz, false) != PB_OK)
    {
        LOG_ERR("Could not extract key data");
        return -PB_ERR;
    }

    if (pb_asn1_ecsig_to_rs(signature, key->kind, &r, &s) != PB_OK)
    {
        LOG_ERR("Could not get r/s values");
        return -PB_ERR;
    }

    memset(caam_tmp_buf, 0, sizeof(caam_tmp_buf));
    memset(caam_ecdsa_key, 0, sizeof(caam_ecdsa_key));
    memcpy(caam_ecdsa_key, key_data, key_sz);
    memcpy(caam_ecdsa_r, r, sizeof(caam_ecdsa_r));
    memcpy(caam_ecdsa_s, s, sizeof(caam_ecdsa_s));

    switch (hash->alg)
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
            return -PB_ERR;
    };

    memcpy(caam_ecdsa_hash, hash->buf, hash_len);

    switch (key->kind)
    {
        case BPAK_KEY_PUB_PRIME256v1:
            caam_sig_type = 2;
        break;
        case BPAK_KEY_PUB_SECP384r1:
            caam_sig_type = 3;
        break;
        case BPAK_KEY_PUB_SECP521r1:
            caam_sig_type = 4;
        break;
        default:
            LOG_ERR("Unknown signature format");
            return -PB_ERR;
    };

    arch_clean_cache_range((uintptr_t) caam_tmp_buf, sizeof(caam_tmp_buf));
    arch_clean_cache_range((uintptr_t) caam_ecdsa_key, sizeof(caam_ecdsa_key));
    arch_clean_cache_range((uintptr_t) caam_ecdsa_r, sizeof(caam_ecdsa_r));
    arch_clean_cache_range((uintptr_t) caam_ecdsa_s, sizeof(caam_ecdsa_s));
    arch_clean_cache_range((uintptr_t) caam_ecdsa_hash, hash_len);

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

    arch_clean_cache_range((uintptr_t) desc, sizeof(desc[0]) * dc);

    return caam_shedule_job_sync(desc);
}



/* Crypto Interface */
int caam_hash_init(struct pb_hash_context *ctx, enum pb_hash_algs pb_alg)
{
    ctx->init = true;
    ctx->alg = pb_alg;

    switch (pb_alg)
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
            return -PB_ERR;
    }

    memset(ctx->buf, 0, block_size);
    arch_clean_cache_range((uintptr_t) ctx->buf, block_size);

    return PB_OK;
}

int caam_hash_update(struct pb_hash_context *ctx, void *buf, size_t size)
{
    uint8_t dc = 0;
    int err = PB_OK;

    if (size)
    {
        arch_clean_cache_range((uintptr_t) buf, size);
    }

    if (!ctx->init)
    {
        err = caam_wait_for_job(desc);

        if (err != PB_OK)
            return err;
    }

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | alg |
        CAAM_ALG_AAI(0);


    if (ctx->init)
    {
        desc[1] |= CAAM_ALG_STATE_INIT;
        LOG_DBG("Init %p, %zu", buf, size);
    }
    else
    {
        desc[1] |= CAAM_ALG_STATE_UPDATE;
        desc[dc++]  = LD_NOIMM(CLASS_2, REG_CTX, block_size);
        desc[dc++]  = (uint32_t) (uintptr_t) ctx->buf;
        LOG_DBG("Update %p, %zu", buf, size);
    }

    ctx->init = false;

    desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    desc[dc++] = (uint32_t) (uintptr_t) buf;
    desc[dc++] = (uint32_t) size;
    desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, block_size);
    desc[dc++] = (uint32_t) (uintptr_t) ctx->buf;

    desc[0] |= dc;

    arch_clean_cache_range((uintptr_t) desc, sizeof(desc[0]) * dc);
    return caam_shedule_job_async(desc);
}

int caam_hash_finalize(struct pb_hash_context *ctx,
                              void *buf, size_t size)
{
    int err;
    int dc = 0;

    LOG_DBG("Buf <%p> %zu", buf, size);

    if (buf && size)
    {
        arch_clean_cache_range((uintptr_t) buf, size);
    }

    err = caam_wait_for_job(desc);

    if (err != PB_OK)
        return err;

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | alg |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_FIN;

    desc[dc++] = LD_NOIMM(CLASS_2, REG_CTX, block_size);
    desc[dc++]  = (uint32_t) (uintptr_t) ctx->buf;
    desc[dc++]  = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    desc[dc++]  = (uint32_t) (uintptr_t) buf;
    desc[dc++]  = (uint32_t) size;
    desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, block_size);
    desc[dc++] = (uint32_t) (uintptr_t) ctx->buf;

    desc[0] |= dc;

    arch_clean_cache_range((uintptr_t) desc, sizeof(desc[0]) * dc);

    err = caam_shedule_job_sync(desc);

    arch_invalidate_cache_range((uintptr_t) ctx->buf, block_size);

#if LOGLEVEL > 2
    if (err == PB_OK)
    {
        printf("Hash: ");
        for (int i = 0; i < block_size/2; i++)
            printf("%02x", ctx->buf[i]);
        printf("\n\r");
    }
#endif

    return err;
}


int caam_pk_verify(struct pb_hash_context *hash,
                    struct bpak_key *key,
                    void *signature, size_t size)
{

    int err = PB_ERR;
    int hash_length;

    switch (hash->alg)
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
        {
            LOG_ERR("Invalid hash: %i", hash->alg);
            return PB_ERR;
        }
    }


    switch (key->kind)
    {
        case BPAK_KEY_PUB_RSA4096:
        {
            LOG_DBG("Checking RSA4096 signature...");
            err = caam_rsa_enc(signature, 512,
                        (uint8_t *) output_data, key);


            if (err != PB_OK)
                break;

            if (memcmp(&output_data[512-hash_length],
                            hash->buf, hash_length) == 0)
                err = PB_OK;
            else
                err = PB_ERR;

            LOG_DBG("Signature %s", (err == PB_OK)?"OK":"Fail");
        }
        break;
        case BPAK_KEY_PUB_PRIME256v1:
        case BPAK_KEY_PUB_SECP384r1:
        case BPAK_KEY_PUB_SECP521r1:
            LOG_DBG("Checking EC signature...");
            err = caam_ecdsa_verify(hash, key, signature);
        break;
        default:
            LOG_ERR("Unknown signature format");
            err = PB_ERR;
        break;
    }

    return err;
}

int imx_caam_init(void)
{
    LOG_INFO("CAAM init %p", (void *) CONFIG_IMX_CAAM_BASE);

    memset(input_ring, 0, sizeof(input_ring[0]) * JOB_RING_ENTRIES);
    memset(output_ring, 0, sizeof(output_ring[0]) * JOB_RING_ENTRIES * 2);

    arch_clean_cache_range((uintptr_t) input_ring,
                sizeof(input_ring[0]) * JOB_RING_ENTRIES);

    arch_clean_cache_range((uintptr_t) output_ring,
                sizeof(output_ring[0]) * JOB_RING_ENTRIES * 2);

    /* Initialize job rings */
    pb_write32((uint32_t)(uintptr_t) input_ring,  CONFIG_IMX_CAAM_BASE + CAAM_IRBAR);
    pb_write32((uint32_t)(uintptr_t) output_ring, CONFIG_IMX_CAAM_BASE + CAAM_ORBAR);

    pb_write32(JOB_RING_ENTRIES, CONFIG_IMX_CAAM_BASE + CAAM_IRSR);
    pb_write32(JOB_RING_ENTRIES, CONFIG_IMX_CAAM_BASE + CAAM_ORSR);

    return PB_OK;
}
