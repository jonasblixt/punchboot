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

#include "desc_defines.h"
#include "desc_helper.h"
#include <arch/arch.h>
#include <bpak/keystore.h>
#include <drivers/crypto/imx_caam.h>
#include <inttypes.h>
#include <pb/crypto.h>
#include <pb/delay.h>
#include <pb/der_helpers.h>
#include <pb/mmio.h>
#include <pb/pb.h>
#include <pb/utils_def.h>
#include <stdio.h>
#include <string.h>

/* Job ring registers, offsets from the job ring's page (4 KiB, 64 KiB on the
 * i.MX8X). The 64-bit base address registers have their two words transposed
 * on a little endian SoC (MCFGR[DWT] = 0): the least-significant word is at +4. */
#define CAAM_IRBAR_JR_LS           0x0004 /* Input ring base address */
#define CAAM_IRSR_JR               0x000c /* Input ring size */
#define CAAM_IRSAR_JR              0x0014 /* Input ring slots available */
#define CAAM_IRJAR_JR              0x001c /* Input ring jobs added */
#define CAAM_ORBAR_JR_LS           0x0024 /* Output ring base address */
#define CAAM_ORSR_JR               0x002c /* Output ring size */
#define CAAM_ORJRR_JR              0x0034 /* Output ring jobs removed */
#define CAAM_ORSFR_JR              0x003c /* Output ring slots full */
#define CAAM_JRSTAR_JR             0x0044 /* Job ring output status */

/* Version registers, aliased in every job ring page */
#define CAAM_CCBVID                0x0fe4
#define CAAM_CHAVID_LS             0x0fec
#define CAAM_CAAMVID_MS            0x0ff8

#define CAAM_ORSFR_ORSF_MASK       0x3ff
#define CAAM_CCBVID_CAAM_ERA(x)    (((x) >> 24) & 0xff)
#define CAAM_CHAVID_LS_MDVID(x)    (((x) >> 12) & 0xf)
#define CAAM_MDVID_LP256           0
#define CAAM_CAAMVID_MS_IP_ID(x)   (((x) >> 16) & 0xffff)
#define CAAM_CAAMVID_MS_MAJ_REV(x) (((x) >> 8) & 0xff)
#define CAAM_CAAMVID_MS_MIN_REV(x) ((x) & 0xff)

#define CAAM_KEY_MAX_LENGTH        256
#define CAAM_SIG_MAX_LENGTH        136
#define CAAM_NO_OF_DESC            16
#define CAAM_HASH_RCTX_MAX_LENGTH  128
#define CAAM_HASH_BLOCK_MAX_SIZE   128
#define CAAM_JOB_TIMEOUT_ms        1000

/* An output ring entry: the address of the job descriptor and its status,
 * in the JRSTAR format. */
struct caam_out_entry {
    uint32_t desc;
    uint32_t status;
};

struct caam {
    uint8_t hash_align_buf[CAAM_HASH_BLOCK_MAX_SIZE]; /* Hash input alignment buffer */
    /* The caam hash context register 'run_ctx' hold the running digest and
     * a 64 bit counter.
     */
    uint8_t run_ctx[CAAM_HASH_RCTX_MAX_LENGTH]; /* Running context for hash algs */
    uint32_t desc[CAAM_NO_OF_DESC]; /* CAAM descriptor buffer */
    uint8_t ecdsa_tmp_buf[CAAM_KEY_MAX_LENGTH]; /* Working buffer used by ECDSA */
    uint8_t ecdsa_key[CAAM_KEY_MAX_LENGTH]; /* EC public key */
    uint8_t ecdsa_r[CAAM_SIG_MAX_LENGTH / 2]; /* First part of EC signature */
    uint8_t ecdsa_s[CAAM_SIG_MAX_LENGTH / 2]; /* Second part of EC signature */
    /* Input/Output job "rings", since we only support one operation at the
     * time there is only one entry.
     *
     * The input holds a pointer to the head of the job descriptor that should
     * be executed.
     */
    uint32_t input; /* Input job */
    struct caam_out_entry output; /* Output job */
    size_t run_ctx_len; /* Length of hash running ctx */
    size_t hash_align_buf_len; /* Bytes available in alignment buffer */
    uint32_t alg; /* Hash alg that being used */
    uint8_t digest_size; /* Hash digest output size */
    uint8_t block_size; /* Hash block size */
    uint16_t id;
    uint8_t ver_maj;
    uint8_t ver_min;
    uintptr_t base;
    bool init; /* Hash init state variable */
    bool caam_initialized;
    bool async_hashing;
};

static struct caam caam;

static void caam_shedule_job_async(void)
{
    caam.input = (uint32_t)(uintptr_t)caam.desc;
    arch_clean_cache_range((uintptr_t)&caam.input, sizeof(caam.input));
    arch_clean_cache_range((uintptr_t)caam.desc, sizeof(caam.desc[0]) * ARRAY_SIZE(caam.desc));
    mmio_write_32(caam.base + CAAM_IRJAR_JR, 1);
}

static int caam_wait_for_job(void)
{
    struct pb_timeout to;

    pb_timeout_init_us(&to, CAAM_JOB_TIMEOUT_ms * 1000);

    /* On a HAB booted device the CAAM stops fetching jobs when the security
     * state machine leaves 'Trusted', e.g. when JTAG is connected. */
    while ((mmio_read_32(caam.base + CAAM_ORSFR_JR) & CAAM_ORSFR_ORSF_MASK) == 0) {
        if (pb_timeout_has_expired(&to)) {
            LOG_ERR("Job timeout, IRSAR = %" PRIu32 ", JRSTAR = %08" PRIx32,
                    mmio_read_32(caam.base + CAAM_IRSAR_JR),
                    mmio_read_32(caam.base + CAAM_JRSTAR_JR));
            return -PB_ERR_TIMEOUT;
        }
    }

    arch_invalidate_cache_range((uintptr_t)&caam.output, sizeof(caam.output));

    if (caam.output.desc != (uint32_t)(uintptr_t)&caam.desc[0]) {
        LOG_ERR("Job failed, status = 0x%x", caam.output.status);
        return -PB_ERR;
    }

    mmio_write_32(caam.base + CAAM_ORJRR_JR, 1);

    uint32_t caam_status = mmio_read_32(caam.base + CAAM_JRSTAR_JR);

    if (caam_status) {
        LOG_ERR("JR status = %08x", caam_status);
        return -PB_ERR;
    }

    return PB_OK;
}

static int caam_shedule_job_sync(void)
{
    caam_shedule_job_async();
    return caam_wait_for_job();
}

static int caam_ecda_verify(const uint8_t *der_signature,
                            size_t sig_length,
                            const uint8_t *der_key,
                            size_t key_length,
                            hash_t md_alg,
                            uint8_t *md,
                            size_t md_length,
                            bool *verified)
{
    int rc;
    int dc = 0;
    uint32_t ecdsel = 0;
    size_t rs_length = 0;
    dsa_t key_kind;

    *verified = false;

    if (!caam.caam_initialized)
        return -PB_ERR_STATE;

    rc = der_ec_public_key_data(der_key, caam.ecdsa_key, sizeof(caam.ecdsa_key), &key_kind);

    if (rc != PB_OK)
        return rc;

    memset(caam.ecdsa_tmp_buf, 0, sizeof(caam.ecdsa_tmp_buf));
    memset(caam.ecdsa_r, 0, CAAM_SIG_MAX_LENGTH / 2);
    memset(caam.ecdsa_s, 0, CAAM_SIG_MAX_LENGTH / 2);

    switch (key_kind) {
    case DSA_EC_SECP256r1:
        ecdsel = PDB_ECDSEL_P256;
        rs_length = 32;
        break;
    case DSA_EC_SECP384r1:
        ecdsel = PDB_ECDSEL_P384;
        rs_length = 48;
        break;
    case DSA_EC_SECP521r1:
        ecdsel = PDB_ECDSEL_P521;
        rs_length = 66;
        break;
    default:
        return -PB_ERR_NOT_SUPPORTED;
    };

    rc = der_ecsig_to_rs(der_signature, sig_length, caam.ecdsa_r, caam.ecdsa_s, rs_length);

    if (rc != PB_OK)
        return rc;

    arch_clean_cache_range((uintptr_t)caam.ecdsa_tmp_buf, sizeof(caam.ecdsa_tmp_buf));
    arch_clean_cache_range((uintptr_t)caam.ecdsa_key, sizeof(caam.ecdsa_key));
    arch_clean_cache_range((uintptr_t)caam.ecdsa_r, sizeof(caam.ecdsa_r));
    arch_clean_cache_range((uintptr_t)caam.ecdsa_s, sizeof(caam.ecdsa_s));
    arch_clean_cache_range((uintptr_t)md, md_length);

    caam.desc[dc++] = DESC_HDR(0);
    /* The PDB, the curve is one of the built-in domains */
    caam.desc[dc++] = PDB_DSA_PD | PDB_DSA_ECDSEL(ecdsel);
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.ecdsa_key;
    caam.desc[dc++] = (uint32_t)(uintptr_t)md;
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.ecdsa_r;
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.ecdsa_s;
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.ecdsa_tmp_buf;
    caam.desc[dc++] = md_length;
    caam.desc[dc++] = CMD_OP_TYPE | OP_TYPE(UNI) | PROTID(DSAVERIFY) | PROT_PK_MES_REP(HASHED) |
                      PROT_PK_ECC;

    /* Start at the OPERATION command, after the PDB */
    caam.desc[0] = DESC_HEADER_IDX(dc, dc - 1);

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
        caam.alg = OP_ALGO(SHA256);
        caam.digest_size = 32;
        caam.block_size = 64;
        caam.run_ctx_len = 32 + sizeof(uint64_t);
        break;
    case HASH_SHA384:
        caam.alg = OP_ALGO(SHA384);
        caam.digest_size = 48;
        caam.block_size = 128;
        caam.run_ctx_len = 64 + sizeof(uint64_t);
        break;
    case HASH_SHA512:
        caam.alg = OP_ALGO(SHA512);
        caam.digest_size = 64;
        caam.block_size = 128;
        caam.run_ctx_len = 64 + sizeof(uint64_t);
        break;
    case HASH_MD5:
        caam.alg = OP_ALGO(MD5);
        caam.digest_size = 16;
        caam.block_size = 64;
        caam.run_ctx_len = 16 + sizeof(uint64_t);
        break;
    case HASH_MD5_BROKEN:
        caam.alg = OP_ALGO(MD5);
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
    arch_clean_cache_range((uintptr_t)caam.run_ctx, caam.run_ctx_len);

    return PB_OK;
}

static int _hash_update(uint8_t *buf, size_t length)
{
    uint8_t dc = 0;
    int err = PB_OK;

    if (!caam.init && caam.async_hashing) {
        /* Block if there is an operation in progress */
        err = caam_wait_for_job();

        if (err != PB_OK)
            return err;
    }

    if (length) {
        arch_clean_cache_range((uintptr_t)buf, length);
    }

    caam.desc[dc++] = DESC_HDR(0);
    caam.desc[dc++] = CMD_OP_TYPE | OP_TYPE(CLASS2) | caam.alg | ALGO_AAI(DIGEST_HASH);

    if (caam.init) {
        caam.desc[1] |= ALGO_AS(INIT);
    } else {
        caam.desc[1] |= ALGO_AS(UPDATE);
        caam.desc[dc++] = LD_NOIMM(CLASS_2, REG_CTX, caam.run_ctx_len);
        caam.desc[dc++] = (uint32_t)(uintptr_t)caam.run_ctx;
    }

    caam.init = false;

    caam.desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    caam.desc[dc++] = (uint32_t)(uintptr_t)buf;
    caam.desc[dc++] = (uint32_t)length;
    caam.desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, caam.run_ctx_len);
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.run_ctx;

    caam.desc[0] |= HDR_JD_DESCLEN(dc);
    caam_shedule_job_async();

    if (!caam.async_hashing) {
        err = caam_wait_for_job();

        if (err != PB_OK)
            return err;
    }

    return PB_OK;
}

static int caam_hash_input(const void *buf, size_t length)
{
    size_t bytes_to_process = length;
    uint8_t *buf_p = (uint8_t *)buf;
    size_t bytes_to_copy;
    size_t no_of_blocks;
    int rc = PB_OK;

    /* We have less then 'block_size' bytes available, fill the buffer */
    if (caam.hash_align_buf_len + bytes_to_process < caam.block_size) {
        // LOG_DBG("Fill1 %zu, %zu", caam.hash_align_buf_len, bytes_to_process);
        memcpy(&caam.hash_align_buf[caam.hash_align_buf_len], buf_p, bytes_to_process);

        caam.hash_align_buf_len += bytes_to_process;
    } else {
        /* Alignment buffer + input holds at least one block */

        /* If there already is data in the buffer we need to append
         * the input data to fill the buffer up to 'block_size' */
        if (caam.hash_align_buf_len > 0) {
            bytes_to_copy = caam.block_size - caam.hash_align_buf_len;
            // LOG_DBG("Fill2 %zu, %zu", caam.hash_align_buf_len, bytes_to_copy);
            memcpy(&caam.hash_align_buf[caam.hash_align_buf_len], buf_p, bytes_to_copy);

            caam.hash_align_buf_len += bytes_to_copy;
            buf_p += bytes_to_copy;
            bytes_to_process -= bytes_to_copy;

            rc = _hash_update(caam.hash_align_buf, caam.hash_align_buf_len);

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
                // LOG_DBG("Fill3 %zu, %zu", caam.hash_align_buf_len, bytes_to_process);
                memcpy(&caam.hash_align_buf[caam.hash_align_buf_len], buf_p, bytes_to_process);
                caam.hash_align_buf_len += bytes_to_process;
            }
        }
    }

    return rc;
}

static int caam_hash_update(const void *buf, size_t length)
{
    caam.async_hashing = false;
    return caam_hash_input(buf, length);
}

static int caam_hash_update_async(const void *buf, size_t length)
{
    caam.async_hashing = true;
    return caam_hash_input(buf, length);
}

static int caam_hash_final(uint8_t *output, size_t size)
{
    int err;
    int dc = 0;

    /* If there was no update we must still call the update function
     * once to initialize the hash block */
    if (caam.init) {
        err = _hash_update(NULL, 0);

        if (err != PB_OK)
            return err;
    }

    if (caam.async_hashing) {
        err = caam_wait_for_job();

        if (err != PB_OK)
            return err;
    }

    arch_clean_cache_range((uintptr_t)output, size);
    arch_clean_cache_range((uintptr_t)caam.hash_align_buf, caam.hash_align_buf_len);

    caam.desc[dc++] = DESC_HDR(0);
    caam.desc[dc++] = CMD_OP_TYPE | OP_TYPE(CLASS2) | caam.alg | ALGO_AAI(DIGEST_HASH) |
                      ALGO_AS(FINAL);

    caam.desc[dc++] = LD_NOIMM(CLASS_2, REG_CTX, caam.run_ctx_len);
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.run_ctx;
    caam.desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.hash_align_buf;
    caam.desc[dc++] = (uint32_t)caam.hash_align_buf_len;
    caam.desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, caam.digest_size);
    caam.desc[dc++] = (uint32_t)(uintptr_t)output;

    caam.desc[0] |= HDR_JD_DESCLEN(dc);

    err = caam_shedule_job_sync();

    arch_invalidate_cache_range((uintptr_t)output, caam.digest_size);

    caam.hash_align_buf_len = 0;

    return err;
}

int imx_caam_init(uintptr_t base)
{
    int rc;

    memset(&caam, 0, sizeof(caam));
    caam.base = base;

    uint32_t caam_version = mmio_read_32(caam.base + CAAM_CAAMVID_MS);
    caam.id = CAAM_CAAMVID_MS_IP_ID(caam_version);
    caam.ver_maj = CAAM_CAAMVID_MS_MAJ_REV(caam_version);
    caam.ver_min = CAAM_CAAMVID_MS_MIN_REV(caam_version);

    LOG_INFO("Base 0x%" PRIxPTR " ID: 0x%04x, ver %i.%i",
             caam.base,
             caam.id,
             caam.ver_maj,
             caam.ver_min);

    arch_clean_cache_range((uintptr_t)&caam.input, sizeof(caam.input));
    arch_clean_cache_range((uintptr_t)&caam.output, sizeof(caam.output));

    /* Initialize job rings, one entry each */
    mmio_write_32(caam.base + CAAM_IRBAR_JR_LS, (uint32_t)(uintptr_t)&caam.input);
    mmio_write_32(caam.base + CAAM_ORBAR_JR_LS, (uint32_t)(uintptr_t)&caam.output);
    mmio_write_32(caam.base + CAAM_IRSR_JR, 1);
    mmio_write_32(caam.base + CAAM_ORSR_JR, 1);

    static struct hash_ops caam_ops = {
        .name = "caam-hash",
        .alg_bits = HASH_MD5 | HASH_MD5_BROKEN | HASH_SHA256 | HASH_SHA384 | HASH_SHA512,
        .init = caam_hash_init,
        .update = caam_hash_update,
        .update_async = caam_hash_update_async,
        .final = caam_hash_final,
    };

    /* The low power MDHA (e.g. i.MX6UL) only does up to SHA-256, and fails
     * SHA-384/512 jobs with a mode error. CHAVID_LS is only valid before era
     * 10, and CCBVID reports the era from era 6, so other CAAMs keep all
     * algorithms. */
    uint32_t era = CAAM_CCBVID_CAAM_ERA(mmio_read_32(caam.base + CAAM_CCBVID));
    uint32_t md_vid = CAAM_CHAVID_LS_MDVID(mmio_read_32(caam.base + CAAM_CHAVID_LS));

    if (era > 0 && era < 10 && md_vid == CAAM_MDVID_LP256) {
        LOG_INFO("Era %" PRIu32 ", MDHA LP256: no SHA-384/512", era);
        caam_ops.alg_bits &= ~(HASH_SHA384 | HASH_SHA512);
    }

    rc = hash_add_ops(&caam_ops);

    if (rc != PB_OK)
        return rc;

    static const struct dsa_ops caam_dsa_ops = {
        .name = "caam-dsa",
        .alg_bits = DSA_EC_SECP256r1 | DSA_EC_SECP384r1 | DSA_EC_SECP521r1,
        .verify = caam_ecda_verify,
    };

    rc = dsa_add_ops(&caam_dsa_ops);

    if (rc != PB_OK)
        return rc;

    caam.caam_initialized = true;
    return PB_OK;
}
