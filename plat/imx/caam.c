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

#define IMX_CAAM_DEV(dev) ((struct imx_caam_device *) drv->private)
#define IMX_CAAM_PRIV(drv) ((struct imx_caam_private *) drv->platform->private)

struct imx_caam_private
{
    struct caam_hash_ctx ctx;
    uint32_t alg;
    uint8_t block_size;
    uint32_t desc[16] __a4k;
    uint8_t hash_ctx[128] __a4k;
    uint8_t caam_tmp_buf[256] __a4k;
    uint8_t caam_ecdsa_key[256] __a4k;
    uint8_t caam_ecdsa_r[66] __a4k;
    uint8_t caam_ecdsa_s[66] __a4k;
    uint8_t caam_ecdsa_hash[128] __a4k;
    char output_data[1024] __a4k;
    uint32_t input_ring[JOB_RING_ENTRIES] __a4k;
    uint32_t output_ring[JOB_RING_ENTRIES*2] __a4k;
};

static int caam_shedule_job_async(struct pb_crypto_driver *drv, uint32_t *job)
{
    struct imx_caam_device *dev = IMX_CAAM_DEV(drv);
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);

    priv->input_ring[0] = (uint32_t)(uintptr_t) job;
    pb_write32(1, dev->base + CAAM_IRJAR);

    return PB_OK;
}

static int caam_shedule_job_sync(struct pb_crypto_driver *drv, uint32_t *job)
{
    int err;
    struct imx_caam_device *dev = IMX_CAAM_DEV(drv);
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);

    err = caam_shedule_job_async(drv, job);

    if (err != PB_OK)
        return err;

    while ((pb_read32(dev->base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    if (priv->output_ring[0] != (uint32_t)(uintptr_t) job)
    {
        LOG_ERR("Job failed");
        return -PB_ERR;
    }

    pb_write32(1, dev->base + CAAM_ORJRR);

    uint32_t caam_status = pb_read32(dev->base + CAAM_JRSTAR);

    if (caam_status)
    {
        LOG_ERR("Job error %08x", caam_status);
        return -PB_ERR;
    }

    return PB_OK;
}

static int caam_wait_for_job(struct pb_crypto_driver *drv, uint32_t *job)
{
    struct imx_caam_device *dev = IMX_CAAM_DEV(drv);
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);

    while ((pb_read32(dev->base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    if (priv->output_ring[0] != (uint32_t)(uintptr_t) job)
    {
        LOG_ERR("Job failed\n\r");
        return -PB_ERR;
    }
    pb_write32(1, dev->base + CAAM_ORJRR);

    uint32_t caam_status = pb_read32(dev->base + CAAM_JRSTAR);

    if (caam_status)
    {
        LOG_ERR("Job error %08x %p", caam_status, job);
        return -PB_ERR;
    }

    return PB_OK;
}

static int caam_rsa_enc(struct pb_crypto_driver *drv, uint8_t *signature,
                        size_t input_sz, uint8_t *output, struct bpak_key *k)
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

static int caam_ecdsa_verify(struct pb_crypto_driver *drv,
                            struct pb_hash_context *hash,
                            struct bpak_key *key,
                            void *signature)
{
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);


    int dc = 0;
    int hash_len = 0;
    int caam_sig_type = 0;
    uint8_t *key_data = NULL;
    size_t key_sz = 0;
    uint8_t *r = NULL;
    uint8_t *s = NULL;

    if (pb_asn1_eckey_data(key, &key_data, &key_sz) != PB_OK)
    {
        LOG_ERR("Could not extract key data");
        return -PB_ERR;
    }

    if (pb_asn1_ecsig_to_rs(signature, key->kind, &r, &s) != PB_OK)
    {
        LOG_ERR("Could not get r/s values");
        return -PB_ERR;
    }

    memset(priv->caam_tmp_buf, 0, 256);
    memset(priv->caam_ecdsa_key, 0, 256);
    memcpy(priv->caam_ecdsa_key, key_data, key_sz);
    memcpy(priv->caam_ecdsa_r, r, 66);
    memcpy(priv->caam_ecdsa_s, s, 66);

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

    memcpy(priv->caam_ecdsa_hash, hash->buf, hash_len);

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

    priv->desc[dc++] = CAAM_CMD_HEADER;
    priv->desc[dc++] = (1 << 22) | (caam_sig_type << 7);
    priv->desc[dc++] = (uint32_t)(uintptr_t) priv->caam_ecdsa_key;
    priv->desc[dc++] = (uint32_t)(uintptr_t) priv->caam_ecdsa_hash;
    priv->desc[dc++] = (uint32_t)(uintptr_t) priv->caam_ecdsa_r;
    priv->desc[dc++] = (uint32_t)(uintptr_t) priv->caam_ecdsa_s;
    priv->desc[dc++] = (uint32_t)(uintptr_t) priv->caam_tmp_buf;
    priv->desc[dc++] = hash_len;
    priv->desc[dc++] = CAAM_CMD_OP | (0x16 << 16) | (2 << 10) | (1 << 1);

    priv->desc[0] |= ((dc-1) << 16) | dc;

    return caam_shedule_job_sync(drv, priv->desc);
}



/* Crypto Interface */
static int caam_hash_init(struct pb_crypto_driver *drv,
                          struct pb_hash_context *ctx,
                          enum pb_hash_algs alg)
{
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);
    ctx->init = true;

    switch (alg)
    {
        case PB_HASH_SHA256:
            priv->alg = CAAM_ALG_TYPE_SHA256;
            priv->block_size = 64;
        break;
        case PB_HASH_SHA384:
            priv->alg = CAAM_ALG_TYPE_SHA384;
            priv->block_size = 96;
        break;
        case PB_HASH_SHA512:
            priv->alg = CAAM_ALG_TYPE_SHA512;
            priv->block_size = 128;
        break;
        case PB_HASH_MD5:
            priv->alg = CAAM_ALG_TYPE_MD5;
            priv->block_size = 16;
        break;
        default:
            return -PB_ERR;
    }

    memset(ctx->buf, 0, priv->block_size);

    return PB_OK;
}

static int caam_hash_update(struct pb_crypto_driver *drv,
                              struct pb_hash_context *ctx,
                              void *buf, size_t size)
{
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);
    uint8_t dc = 0;
    int err = PB_OK;

    if (!ctx->init)
    {
        err = caam_wait_for_job(drv, priv->desc);

        if (err != PB_OK)
            return err;
    }

    priv->desc[dc++] = CAAM_CMD_HEADER;
    priv->desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | priv->alg |
        CAAM_ALG_AAI(0);


    if (ctx->init)
    {
        priv->desc[1] |= CAAM_ALG_STATE_INIT;
        LOG_DBG("Init %p, %lu", buf, size);
    }
    else
    {
        priv->desc[1] |= CAAM_ALG_STATE_UPDATE;
        priv->desc[dc++]  = LD_NOIMM(CLASS_2, REG_CTX, priv->block_size);
        priv->desc[dc++]  = (uint32_t) (uintptr_t) ctx->buf;
        LOG_DBG("Update %p, %lu", buf, size);
    }

    ctx->init = false;

    priv->desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    priv->desc[dc++] = (uint32_t) (uintptr_t) buf;
    priv->desc[dc++] = (uint32_t) size;
    priv->desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, priv->block_size);
    priv->desc[dc++] = (uint32_t) (uintptr_t) ctx->buf;

    priv->desc[0] |= dc;

    return caam_shedule_job_async(drv, priv->desc);
}

static int caam_hash_finalize(struct pb_crypto_driver *drv,
                              struct pb_hash_context *ctx,
                              void *buf, size_t size)
{
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);
    int err;
    int dc = 0;

    err = caam_wait_for_job(drv, priv->desc);

    if (err != PB_OK)
        return err;

    priv->desc[dc++] = CAAM_CMD_HEADER;
    priv->desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | priv->alg |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_FIN;

    priv->desc[dc++] = LD_NOIMM(CLASS_2, REG_CTX, priv->block_size);
    priv->desc[dc++]  = (uint32_t) (uintptr_t) ctx->buf;
    priv->desc[dc++]  = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    priv->desc[dc++]  = (uint32_t) (uintptr_t) buf;
    priv->desc[dc++]  = (uint32_t) size;
    priv->desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, priv->block_size);
    priv->desc[dc++] = (uint32_t) (uintptr_t) ctx->buf;

    priv->desc[0] |= dc;

    err = caam_shedule_job_sync(drv, priv->desc);

#if LOGLEVEL > 1
    if (err == PB_OK)
    {
        printf("Hash: ");
        for (int i = 0; i < priv->block_size/2; i++)
            printf("%02x", ctx->buf[i]);
        printf("\n\r");
    }
#endif

    return err;
}


static int caam_pk_verify(struct pb_crypto_driver *drv,
                            struct pb_hash_context *hash,
                            struct bpak_key *key,
                            void *signature, size_t size)
{
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);

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
            return PB_ERR;
    }


    switch (key->kind)
    {
        case BPAK_KEY_PUB_RSA4096:
        {
            LOG_DBG("Checking RSA4096 signature...");
            err = caam_rsa_enc(drv, signature, 512,
                        (uint8_t *) priv->output_data, key);


            if (err != PB_OK)
                break;

            if (memcmp(&priv->output_data[512-hash_length],
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
            err = caam_ecdsa_verify(drv, hash, key, signature);
        break;
        default:
            LOG_ERR("Unknown signature format");
            err = PB_ERR;
        break;
    }

    return err;
}

int imx_caam_init(struct pb_crypto_driver *drv)
{
    struct imx_caam_device *dev = IMX_CAAM_DEV(drv);
    struct imx_caam_private *priv = IMX_CAAM_PRIV(drv);

    LOG_INFO("CAAM init %p", (void *) dev->base);

    for (int n = 0; n < JOB_RING_ENTRIES; n++)
        priv->input_ring[n] = 0;

    for (int n = 0; n < JOB_RING_ENTRIES*2; n++)
        priv->output_ring[n] = 0;

    /* Initialize job rings */
    pb_write32((uint32_t)(uintptr_t) priv->input_ring,  dev->base + CAAM_IRBAR);
    pb_write32((uint32_t)(uintptr_t) priv->output_ring, dev->base + CAAM_ORBAR);

    pb_write32(JOB_RING_ENTRIES, dev->base + CAAM_IRSR);
    pb_write32(JOB_RING_ENTRIES, dev->base + CAAM_ORSR);

    drv->ready = true;
    drv->hash_init = caam_hash_init;
    drv->hash_update = caam_hash_update;
    drv->hash_final = caam_hash_finalize;
    drv->pk_verify = caam_pk_verify;

    return PB_OK;
}

int imx_caam_free(struct pb_crypto_driver *drv)
{
    drv->ready = false;
    return PB_OK;
}
