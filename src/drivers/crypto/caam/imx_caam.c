/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *
 * This module implements a simple interface to the NXP CAAM block. It only
 * supports one operation at the time, for example it's undefined to have
 * a running hash context while starting a signature verification operation.
 *
 * Limitations:
 *  - Only one hash context is supported
 *  - It's not supported to have a running hash context when starting
 *      a signature verification
 *
 * Notes on hash data input alignment:
 *
 *  The update function will align input data to the block size of the
 *  currently running hash. However, if for example the first update is not
 *  aligned and any data that is inputed after that is all data will go through
 *  the alignment buffer, which has a huge performance impact.
 *
 *  For maximum performance the input data should be block aligned. The exception
 *  is if the last portion of the input data is not, that will not have a 
 *  significant inpact on performance as it is one extra memcpy operation.
 *
 *  Notes on the hash update function:
 *
 *  The update operation sets up a descriptor and the CAAM hardware uses
 *  DMA. If there are no running operations the update function will
 *  return as soon as the job has been submitted to the CAAM job queue.
 *  If there is an update operation already in progress the update function
 *  will block until it's done.
 *
 *  This way the update function can be called in an interleaved way when,
 *  for example reading chunks from a block device, calling the update function
 *  and then repeat until all data has been processed. This means that DMA
 *  transfers from a block device can run simultanious with DMA transfers
 *  to the CAAM.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <mmio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <bpak/keystore.h>
#include <plat/defs.h>
#include <crypto.h>
#include <der_helpers.h>
#include <drivers/crypto/imx_caam.h>
#include "desc_defines.h"
#include "desc_helper.h"

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

/* Registers */
#define CAAM_IRBAR          0x0004
#define CAAM_IRSR           0x000c
#define CAAM_IRSAR          0x0014
#define CAAM_IRJAR          0x001c
#define CAAM_ORBAR          0x0024
#define CAAM_ORSR           0x002c
#define CAAM_ORJRR          0x0034
#define CAAM_ORSFR          0x003c
#define CAAM_JRSTAR         0x0044
#define CAAM_JRINTR         0x004c
#define CAAM_JRCFGR_MS      0x0050
#define CAAM_JRCFGR_LS      0x0054
#define CAAM_IRRIR          0x005c
#define CAAM_ORWIR          0x0064
#define CAAM_JRCR           0x006c
#define CAAM_SMCJR          0x00f4
#define CAAM_SMCSJR         0x00fc
#define CAAM_VID_MS         0x0ff8
#define CAAM_VID_LS         0x0ffc

#define CAAM_KEY_MAX_LENGTH 256
#define CAAM_SIG_MAX_LENGTH 136
#define CAAM_NO_OF_DESC 16
#define CAAM_HASH_RCTX_MAX_LENGTH 128
#define CAAM_HASH_BLOCK_MAX_SIZE 128

struct caam {
    uint8_t hash_align_buf[CAAM_HASH_BLOCK_MAX_SIZE]; /* Hash input alignment buffer */
    /* The caam hash context register 'run_ctx' hold the running digest and
     * a 64 bit counter.
     */
    uint8_t run_ctx[CAAM_HASH_RCTX_MAX_LENGTH]; /* Running context for hash algs */
    uint32_t desc[CAAM_NO_OF_DESC];             /* CAAM descriptor buffer */
    uint8_t ecdsa_tmp_buf[CAAM_KEY_MAX_LENGTH]; /* Working buffer used by ECDSA */
    uint8_t ecdsa_key[CAAM_KEY_MAX_LENGTH];     /* EC public key */
    uint8_t ecdsa_r[CAAM_SIG_MAX_LENGTH/2];     /* First part of EC signature */
    uint8_t ecdsa_s[CAAM_SIG_MAX_LENGTH/2];     /* Second part of EC signature */
    /* Input/Output job "rings", since we only support one operation at the
     * time there is only one entry.
     *
     * The input holds a pointer to the head of the job descriptor that should
     * be executed.
     *
     * The output ring holds two 32-bit words, the first is the pointer to
     * the executed job and the second word is status information
     */
    uint32_t input;                 /* Input job */
    uint32_t output[2];             /* Output job */
    size_t run_ctx_len;             /* Length of hash running ctx */
    size_t hash_align_buf_len;      /* Bytes available in alignment buffer */
    uint32_t alg;                   /* Hash alg that being used */
    uint8_t digest_size;            /* Hash digest output size */
    uint8_t block_size;             /* Hash block size */
    uint16_t id;
    uint8_t ver_maj;
    uint8_t ver_min;
    uintptr_t base;
    bool init;                      /* Hash init state variable */
    bool caam_initialized;
};

static struct caam caam;

static void caam_shedule_job_async(void)
{
    caam.input = (uint32_t)(uintptr_t) caam.desc;
    arch_clean_cache_range((uintptr_t) &caam.input, sizeof(caam.input));
    arch_clean_cache_range((uintptr_t) caam.desc,
                           sizeof(caam.desc[0]) * membersof(caam.desc));
    mmio_write_32(caam.base + CAAM_IRJAR, 1);
}

static int caam_shedule_job_sync(void)
{
    caam_shedule_job_async();

    while ((mmio_read_32(caam.base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    arch_invalidate_cache_range((uintptr_t) &caam.output[0], sizeof(caam.output)*2);

    if (caam.output[0] != (uint32_t)(uintptr_t) &caam.desc[0]) {
        LOG_ERR("Job failed");
        return -PB_ERR;
    }

    mmio_write_32(caam.base + CAAM_ORJRR, 1);

    uint32_t caam_status = mmio_read_32(caam.base + CAAM_JRSTAR);

    if (caam_status) {
        LOG_ERR("Job error %08x", caam_status);
        return -PB_ERR;
    }

    return PB_OK;
}

static int caam_wait_for_job(void)
{
    while ((mmio_read_32(caam.base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    arch_invalidate_cache_range((uintptr_t) &caam.output[0], sizeof(caam.output)*2);

    if (caam.output[0] != (uint32_t)(uintptr_t) &caam.desc[0]) {
        LOG_ERR("Job failed");
        return -PB_ERR;
    }

    mmio_write_32(caam.base + CAAM_ORJRR, 1);

    uint32_t caam_status = mmio_read_32(caam.base + CAAM_JRSTAR);

    if (caam_status) {
        LOG_ERR("JR status = %08x", caam_status);
        return -PB_ERR;
    }

    return PB_OK;
}

static int caam_ecda_verify(uint8_t *der_signature,
                            uint8_t *der_key,
                            uint8_t *md, size_t md_length,
                            bool *verified)
{
    int rc;
    int dc = 0;
    int caam_sig_type = 0;
    dsa_t key_kind;

    *verified = false;

    if (!caam.caam_initialized)
        return -PB_ERR_STATE;

    rc = der_ec_public_key_data(der_key,
                                caam.ecdsa_key, sizeof(caam.ecdsa_key),
                                &key_kind);

    if (rc != PB_OK)
        return rc;

    rc = der_ecsig_to_rs(der_signature,
                         caam.ecdsa_r, caam.ecdsa_s, CAAM_SIG_MAX_LENGTH/2,
                         true);

    if (rc != PB_OK)
        return rc;

    memset(caam.ecdsa_tmp_buf, 0, sizeof(caam.ecdsa_tmp_buf));

    switch (key_kind) {
        case DSA_EC_SECP256r1:
            caam_sig_type = 2;
        break;
        case DSA_EC_SECP384r1:
            caam_sig_type = 3;
        break;
        case DSA_EC_SECP521r1:
            caam_sig_type = 4;
        break;
        default:
            return -PB_ERR_NOT_SUPPORTED;
    };

    arch_clean_cache_range((uintptr_t) caam.ecdsa_tmp_buf,
                            sizeof(caam.ecdsa_tmp_buf));
    arch_clean_cache_range((uintptr_t) caam.ecdsa_key, sizeof(caam.ecdsa_key));
    arch_clean_cache_range((uintptr_t) caam.ecdsa_r, sizeof(caam.ecdsa_r));
    arch_clean_cache_range((uintptr_t) caam.ecdsa_s, sizeof(caam.ecdsa_s));
    arch_clean_cache_range((uintptr_t) md, md_length);

    caam.desc[dc++] = CAAM_CMD_HEADER;
    caam.desc[dc++] = (1 << 22) | (caam_sig_type << 7);
    caam.desc[dc++] = (uint32_t)(uintptr_t) caam.ecdsa_key;
    caam.desc[dc++] = (uint32_t)(uintptr_t) md;
    caam.desc[dc++] = (uint32_t)(uintptr_t) caam.ecdsa_r;
    caam.desc[dc++] = (uint32_t)(uintptr_t) caam.ecdsa_s;
    caam.desc[dc++] = (uint32_t)(uintptr_t) caam.ecdsa_tmp_buf;
    caam.desc[dc++] = md_length;
    caam.desc[dc++] = CAAM_CMD_OP | (0x16 << 16) | (2 << 10) | (1 << 1);

    caam.desc[0] |= ((dc-1) << 16) | dc;

    rc = caam_shedule_job_sync();

    if (rc == 0)
        *verified = true;

    return rc;
}

static int caam_hash_init(hash_t pb_alg)
{
    if (!caam.caam_initialized)
        return -PB_ERR_STATE;

    caam.init = true;
    caam.hash_align_buf_len = 0;

    switch (pb_alg) {
        case HASH_SHA256:
            caam.alg = CAAM_ALG_TYPE_SHA256;
            caam.digest_size = 32;
            caam.block_size = 64;
            caam.run_ctx_len = 32 + sizeof(uint64_t);
        break;
        case HASH_SHA384:
            caam.alg = CAAM_ALG_TYPE_SHA384;
            caam.digest_size = 48;
            caam.block_size = 128;
            caam.run_ctx_len = 64 + sizeof(uint64_t);
        break;
        case HASH_SHA512:
            caam.alg = CAAM_ALG_TYPE_SHA512;
            caam.digest_size = 64;
            caam.block_size = 128;
            caam.run_ctx_len = 64 + sizeof(uint64_t);
        break;
        case HASH_MD5:
            caam.alg = CAAM_ALG_TYPE_MD5;
            caam.digest_size = 16;
            caam.block_size = 64;
            caam.run_ctx_len = 16 + sizeof(uint64_t);
        break;
        case HASH_MD5_BROKEN:
            caam.alg = CAAM_ALG_TYPE_MD5;
            /* Setting 'run_ctx_len' to 16 here is on purpouse, because
             * of being compatible with some versions already in the filed
             * the 'BROKEN' version of MD5 has been added with this quirk */
            caam.run_ctx_len = 16;
            caam.digest_size = 16;
            caam.block_size = 64;
        break;
        default:
            LOG_ERR("Unknown pb_alg value 0x%x", pb_alg);
            return -PB_ERR_PARAM;
    }

    memset(caam.run_ctx, 0, caam.run_ctx_len);
    arch_clean_cache_range((uintptr_t) caam.run_ctx, caam.run_ctx_len);

    return PB_OK;
}

static int _hash_update(uint8_t * buf, size_t length)
{
    uint8_t dc = 0;
    int err = PB_OK;

    if (!caam.init) {
        /* Block if there is an operation in progress */
        err = caam_wait_for_job();

        if (err != PB_OK)
            return err;
    }

    if (length) {
        arch_clean_cache_range((uintptr_t) buf, length);
    }

    caam.desc[dc++] = CAAM_CMD_HEADER;
    caam.desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | caam.alg | CAAM_ALG_AAI(0);

    if (caam.init) {
        caam.desc[1] |= CAAM_ALG_STATE_INIT;
    } else {
        caam.desc[1] |= CAAM_ALG_STATE_UPDATE;
        caam.desc[dc++]  = LD_NOIMM(CLASS_2, REG_CTX, caam.run_ctx_len);
        caam.desc[dc++]  = (uint32_t) (uintptr_t) caam.run_ctx;
    }

    caam.init = false;

    caam.desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    caam.desc[dc++] = (uint32_t) (uintptr_t) buf;
    caam.desc[dc++] = (uint32_t) length;
    caam.desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, caam.run_ctx_len);
    caam.desc[dc++] = (uint32_t) (uintptr_t) caam.run_ctx;

    caam.desc[0] |= dc;
    caam_shedule_job_async();

    return PB_OK;
}

static int caam_hash_update(uintptr_t buf, size_t length)
{
    size_t bytes_to_process = length;
    uint8_t *buf_p = (uint8_t *) buf;
    size_t bytes_to_copy;
    size_t no_of_blocks;
    int rc = PB_OK;

    /* We have less then 'block_size' bytes available, fill the buffer */
    if (caam.hash_align_buf_len + bytes_to_process < caam.block_size) {
        memcpy(&caam.hash_align_buf[caam.hash_align_buf_len],
               buf_p,
               bytes_to_process);

        caam.hash_align_buf_len += bytes_to_process;
    } else {
        /* Alignment buffer + input holds at least one block */

        /* If there already is data in the buffer we need to append
         * the input data to fill the buffer up to 'block_size' */
        if (caam.hash_align_buf_len > 0) {
            bytes_to_copy = caam.hash_align_buf_len - caam.block_size;

            memcpy(&caam.hash_align_buf[caam.hash_align_buf_len],
                   buf_p,
                   bytes_to_copy);

            caam.hash_align_buf_len += bytes_to_copy;
            buf_p += bytes_to_copy;
            bytes_to_process -= bytes_to_copy;

            rc = _hash_update(caam.hash_align_buf,
                             caam.hash_align_buf_len);

            if (rc != 0)
                return rc;

            caam.hash_align_buf_len = 0;
        }

        /* Any leftover data? */
        if (bytes_to_process > 0) {
            no_of_blocks = bytes_to_process / caam.block_size;

            /* Check if we have at least one 'block_size' worth of data
             *  in the input buffer */
            if (no_of_blocks > 0) {
                rc = _hash_update(buf_p, no_of_blocks * caam.block_size);

                if (rc != 0)
                    return rc;

                bytes_to_process -= no_of_blocks * caam.block_size;
                buf_p += no_of_blocks * caam.block_size;
            }

            /* Any remaining data is not block aligned and will fit
             *  in the alignment buffer */
            if (bytes_to_process > 0) {
                memcpy(&caam.hash_align_buf[caam.hash_align_buf_len],
                       buf_p,
                       bytes_to_process);
                caam.hash_align_buf_len += bytes_to_process;
            }
        }
    }

    return rc;
}

static int caam_hash_final(uint8_t *output, size_t size)
{
    int err;
    int dc = 0;

    if (caam.init) {
        _hash_update(NULL, 0);
    }

    arch_clean_cache_range((uintptr_t) output, size);
    arch_clean_cache_range((uintptr_t) caam.hash_align_buf,
                                       caam.hash_align_buf_len);

    caam_wait_for_job();

    caam.desc[dc++] = CAAM_CMD_HEADER;
    caam.desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | caam.alg |
                                    CAAM_ALG_AAI(0) | CAAM_ALG_STATE_FIN;

    caam.desc[dc++] = LD_NOIMM(CLASS_2, REG_CTX, caam.run_ctx_len);
    caam.desc[dc++] = (uint32_t) (uintptr_t) caam.run_ctx;
    caam.desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    caam.desc[dc++] = (uint32_t) (uintptr_t) caam.hash_align_buf;
    caam.desc[dc++] = (uint32_t) caam.hash_align_buf_len;
    caam.desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, caam.digest_size);
    caam.desc[dc++] = (uint32_t) (uintptr_t) output;

    caam.desc[0] |= dc;

    err = caam_shedule_job_sync();

    arch_invalidate_cache_range((uintptr_t) output, caam.digest_size);

    caam.hash_align_buf_len = 0;

    return err;
}

int imx_caam_init(uintptr_t base)
{
    int rc;

    memset(&caam, 0, sizeof(caam));
    caam.base = base;

    uint32_t caam_version = mmio_read_32(caam.base + CAAM_VID_MS);
    caam.id = (caam_version >> 16) & 0xfff;
    caam.ver_maj = (caam_version >> 8) & 0xff;
    caam.ver_min = caam_version & 0xff;

    LOG_INFO("Base 0x%"PRIxPTR" ID: 0x%04x, ver %i.%i", caam.base,
                                                      caam.id,
                                                      caam.ver_maj,
                                                      caam.ver_min);

    arch_clean_cache_range((uintptr_t) &caam.input, sizeof(caam.input));
    arch_clean_cache_range((uintptr_t) caam.output, sizeof(caam.output)*2);

    /* Initialize job rings */
    mmio_write_32(caam.base + CAAM_IRBAR, (uint32_t)(uintptr_t) &caam.input);
    mmio_write_32(caam.base + CAAM_ORBAR, (uint32_t)(uintptr_t) caam.output);

    /* Program number of input/output job entries */
    mmio_write_32(caam.base + CAAM_IRSR, 1);
    mmio_write_32(caam.base + CAAM_ORSR, 1);

    static const struct hash_ops caam_ops = {
        .name = "caam-hash",
        .alg_bits = HASH_MD5 |
                    HASH_MD5_BROKEN |
                    HASH_SHA256 |
                    HASH_SHA384 |
                    HASH_SHA512,
        .init = caam_hash_init,
        .update = caam_hash_update,
        .final = caam_hash_final,
    };

    rc = hash_add_ops(&caam_ops);

    if (rc != PB_OK)
        return rc;

    static const struct dsa_ops caam_dsa_ops = {
        .name = "caam-dsa",
        .alg_bits = DSA_EC_SECP256r1 |
                    DSA_EC_SECP384r1 |
                    DSA_EC_SECP521r1,
        .verify = caam_ecda_verify,
    };

    rc = dsa_add_ops(&caam_dsa_ops);

    if (rc != PB_OK)
        return rc;

    caam.caam_initialized = true;
    return PB_OK;
}
