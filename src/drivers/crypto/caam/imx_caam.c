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

/* Version registers, aliased in every job ring page */
#define CAAM_RNG_VERSION           0x0ef8 /* Era 10 and later */
#define CAAM_CTPR_MS               0x0fa8

/* RNG4 registers, in the controller page (page 0) */
#define CAAM_RTMCTL                0x0600 /* TRNG miscellaneous control */
#define CAAM_RTSCMISC              0x0604 /* Statistical check miscellaneous */
#define CAAM_RTPKRRNG              0x0608 /* Poker range */
#define CAAM_RTPKRMAX              0x060c /* Poker maximum limit */
#define CAAM_RTSDCTL               0x0610 /* Seed control */
#define CAAM_RTFRQMIN              0x0618 /* Frequency count minimum */
#define CAAM_RTFRQMAX              0x061c /* Frequency count maximum */
#define CAAM_RTSCML                0x0620 /* Monobit limits */
#define CAAM_RTSCR1L               0x0624 /* Run length 1 limits */
#define CAAM_RTSCR2L               0x0628
#define CAAM_RTSCR3L               0x062c
#define CAAM_RTSCR4L               0x0630
#define CAAM_RTSCR5L               0x0634
#define CAAM_RTSCR6PL              0x0638
#define CAAM_RDSTA                 0x06c0 /* RNG DRNG status */

#define CAAM_RTMCTL_SAMP_MODE_MASK 0x3
#define CAAM_RTMCTL_SAMP_MODE_RAW  0x1 /* Raw data in the entropy shifter and the checker */
#define CAAM_RTMCTL_ACC            BIT(5)
#define CAAM_RTMCTL_ERR            BIT(12)
#define CAAM_RTMCTL_PRGM           BIT(16)
#define CAAM_RTSDCTL_ENT_DLY(x)    (((x) & 0xffff) << 16)
#define CAAM_RTSDCTL_DLY_GET(x)    (((x) >> 16) & 0xffff)
#define CAAM_RTSDCTL_SAMP_SIZE     512
#define CAAM_RTSCMISC_RTY(x)       ((x) << 16) /* Statistical check retries */
#define CAAM_RTSCMISC_LRUN(x)      (x) /* Longest run */
#define CAAM_RTSC_LIM(range, max)  (((range) << 16) | (max)) /* RTSCML, RTSCRnL */
#define CAAM_RDSTA_IF(sh)          BIT(sh) /* State handle instantiated */
#define CAAM_RDSTA_PR(sh)          BIT(4 + (sh)) /* ... with prediction resistance */
#define CAAM_RDSTA_SKVN            BIT(30) /* Secure keys generated */
#define CAAM_CTPR_MS_RNG_I(x)      (((x) >> 8) & 0x7)
#define CAAM_CHAVID_LS_RNGVID(x)   (((x) >> 16) & 0xf)
#define CAAM_RNG_VERSION_VID(x)    (((x) >> 24) & 0xff)
#define CAAM_RNG_SH_MAX            2

/* Entropy delay, in TRNG clocks per sample */
#define CAAM_RNG_ENT_DLY_MIN       3200
#define CAAM_RNG_ENT_DLY_MAX       12800
#define CAAM_RNG_ENT_DLY_STEP      200

#define CAAM_ORSFR_ORSF_MASK       0x3ff
/* A job's status word: its source, and for the DECO the error's id. 0x86 is
 * a signature verification's "invalid signature" (as NXP's OP-TEE and Linux
 * drivers read it) */
#define CAAM_STATUS_SRC(x)         (((x) >> 28) & 0xf)
#define CAAM_STATUS_SRC_DECO       4
#define CAAM_STATUS_ERR_ID(x)      ((x) & 0xff)
#define CAAM_DECO_INVALID_SIG      0x86
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
    bool job_pending; /* A job is in the ring: added, its completion not taken out */
    int hash_rc; /* What failed the hash that is open, or PB_OK */
};

static struct caam caam;

/* The status of the job the last wait ended, from its output ring entry */
static uint32_t caam_job_status;

/* The engine stores the digest here, in a line of the driver's own, and
 * hash_final() copies it to the caller, whose buffer is often on the stack
 * (crypto.md 3.5, drivers.md 4.2 and 4.3) */
static uint8_t caam_digest[CRYPTO_MD_MAX_SZ] __aligned(64);

/* The engine's address of [p, p + length). The rings and the descriptors
 * hold 32 bits: memory that isn't all below 4 GiB is refused, not cut
 * (drivers.md 4.2). The one place a pointer of the caller's becomes the
 * engine's address; the driver's own memory is checked in imx_caam_init() */
static int caam_addr(const void *p, size_t length, uint32_t *addr)
{
    uint64_t start = (uint64_t)(uintptr_t)p;

    if (start > UINT32_MAX || (uint64_t)length > ((uint64_t)1 << 32) - start) {
        LOG_ERR("%zu bytes at 0x%" PRIxPTR ": not below 4 GiB", length, (uintptr_t)p);
        return -PB_ERR_PARAM;
    }

    *addr = (uint32_t)start;
    return PB_OK;
}

/* A cache line, no smaller than the line of any core pb runs on
 * (drivers.md 4.1, PB_DMA_ALIGN) */
#define CAAM_RING_ALIGN 64

/* Input/Output job "rings", since we only support one operation at the
 * time there is only one entry.
 *
 * The input holds a pointer to the head of the job descriptor that should
 * be executed. The CAAM writes the output entry: each entry owns its lines
 * (drivers.md 4.2, 3 and 5), so that no store of the CPU's to the driver's
 * state makes the output's line dirty while the CAAM writes it. A dirty line
 * written back over the entry reads as a completion of another descriptor */
static struct {
    uint32_t input __aligned(CAAM_RING_ALIGN); /* Input job */
    struct caam_out_entry output __aligned(CAAM_RING_ALIGN); /* Output job */
} caam_ring __aligned(CAAM_RING_ALIGN);

static void caam_shedule_job_async(void)
{
    caam_ring.input = (uint32_t)(uintptr_t)caam.desc;
    arch_clean_cache_range((uintptr_t)&caam_ring.input, sizeof(caam_ring.input));
    arch_clean_cache_range((uintptr_t)caam.desc, sizeof(caam.desc[0]) * ARRAY_SIZE(caam.desc));
    mmio_write_32(caam.base + CAAM_IRJAR_JR, 1);
    caam.job_pending = true;
}

static int caam_wait_for_job(void)
{
    struct pb_timeout to;
    uint32_t desc;
    uint32_t status;

    pb_timeout_init_us(&to, CAAM_JOB_TIMEOUT_ms * 1000);

    do {
        /* On a HAB booted device the CAAM stops fetching jobs when the
         * security state machine leaves 'Trusted', e.g. when JTAG is
         * connected. */
        while ((mmio_read_32(caam.base + CAAM_ORSFR_JR) & CAAM_ORSFR_ORSF_MASK) == 0) {
            if (pb_timeout_has_expired(&to)) {
                LOG_ERR("Job timeout, IRSAR = %" PRIu32 ", JRSTAR = %08" PRIx32,
                        mmio_read_32(caam.base + CAAM_IRSAR_JR),
                        mmio_read_32(caam.base + CAAM_JRSTAR_JR));
                return -PB_ERR_TIMEOUT;
            }
        }

        arch_invalidate_cache_range((uintptr_t)&caam_ring.output, sizeof(caam_ring.output));

        /* The entry says whose it is, by its descriptor, and has the job's
         * status. JRSTAR has the status of whichever job completed last. The
         * entry is taken out of the ring whatever it says, or it would
         * answer the next wait */
        desc = caam_ring.output.desc;
        status = caam_ring.output.status;

        mmio_write_32(caam.base + CAAM_ORJRR_JR, 1);

        /* An entry of another descriptor isn't this job's completion: an
         * earlier user of the job ring left it. The job is still in flight,
         * and its own entry is waited for within the same time */
        caam.job_pending = (desc != (uint32_t)(uintptr_t)&caam.desc[0]);

        if (caam.job_pending) {
            LOG_ERR("Completion of another descriptor, %08" PRIx32 ", status = %08" PRIx32,
                    desc,
                    status);
        }
    } while (caam.job_pending);

    caam_job_status = status;

    if (status) {
        LOG_ERR("Job status = %08" PRIx32, status);
        return -PB_ERR;
    }

    return PB_OK;
}

/* Ends a job that was given up: it is waited for, and its completion is
 * taken out of the ring whatever it says. When this returns PB_OK the engine
 * is idle, and the completion of the next job is the only one in the ring
 * (crypto.md 3.1 and 4). A job that doesn't end is -PB_ERR_TIMEOUT: nothing
 * can start. */
static int caam_idle(void)
{
    if (!caam.job_pending)
        return PB_OK;

    if (caam_wait_for_job() == -PB_ERR_TIMEOUT)
        return -PB_ERR_TIMEOUT;

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
    uint32_t md_addr;

    *verified = false;

    if (!caam.caam_initialized)
        return -PB_ERR_STATE;

    rc = caam_addr(md, md_length, &md_addr);

    if (rc != PB_OK)
        return rc;

    /* The verdict is this job's: nothing else is in the ring */
    rc = caam_idle();

    if (rc != PB_OK)
        return rc;

    /* Nothing of the key before: the engine reads the curve's length */
    memset(caam.ecdsa_key, 0, sizeof(caam.ecdsa_key));

    rc = der_ec_public_key_data(
        der_key, key_length, caam.ecdsa_key, sizeof(caam.ecdsa_key), &key_kind);

    /* A curve no backend has, or a key that can't be parsed (crypto.md 4) */
    if (rc == -PB_ERR_NOT_IMPLEMENTED)
        return -PB_ERR_NOT_SUPPORTED;
    if (rc != PB_OK)
        return -PB_ERR_ASN1;

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

    /* An r or s larger than the curve's order is no valid signature for this
     * key, as the software backend answers it; a signature that can't be
     * parsed is -PB_ERR_ASN1 (crypto.md 4) */
    if (rc == -PB_ERR_BUF_TOO_SMALL)
        return -PB_ERR_SIGNATURE;
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
    caam.desc[dc++] = md_addr;
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.ecdsa_r;
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.ecdsa_s;
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.ecdsa_tmp_buf;
    caam.desc[dc++] = md_length;
    caam.desc[dc++] = CMD_OP_TYPE | OP_TYPE(UNI) | PROTID(DSAVERIFY) | PROT_PK_MES_REP(HASHED) |
                      PROT_PK_ECC;

    /* Start at the OPERATION command, after the PDB */
    caam.desc[0] = DESC_HEADER_IDX(dc, dc - 1);

    rc = caam_shedule_job_sync();

    if (rc == PB_OK) {
        *verified = true;
        return PB_OK;
    }

    /* The engine's verdict, or an error of the engine (crypto.md 4) */
    if (rc == -PB_ERR) {
        if (CAAM_STATUS_SRC(caam_job_status) == CAAM_STATUS_SRC_DECO &&
            CAAM_STATUS_ERR_ID(caam_job_status) == CAAM_DECO_INVALID_SIG)
            return -PB_ERR_SIGNATURE;
        return -PB_ERR_IO;
    }

    return rc;
}

/* Waits for the hash's job when one is in flight: after this its buffer is
 * the caller's again. An error of the job is returned here and by every
 * later call of this hash, which is failed (crypto.md 3.3) */
static int caam_hash_join(void)
{
    if (caam.job_pending) {
        int rc = caam_wait_for_job();

        if (rc != PB_OK)
            caam.hash_rc = rc;
    }

    return caam.hash_rc;
}

static int caam_hash_init(hash_t pb_alg)
{
    int rc;

    if (!caam.caam_initialized)
        return -PB_ERR_STATE;

    /* Whatever hash was open is ended, also one that was given up with its
     * job in flight */
    rc = caam_idle();

    if (rc != PB_OK)
        return rc;

    caam.init = true;
    caam.async_hashing = false;
    caam.hash_rc = PB_OK;
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
    uint32_t addr;

    /* One job at a time: the descriptor and the running context are the
     * job's until it is done */
    err = caam_hash_join();

    if (err != PB_OK)
        return err;

    /* Bytes the engine can't reach fail the hash: no digest without them */
    err = caam_addr(buf, length, &addr);

    if (err != PB_OK) {
        caam.hash_rc = err;
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
    caam.desc[dc++] = addr;
    caam.desc[dc++] = (uint32_t)length;
    caam.desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, caam.run_ctx_len);
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam.run_ctx;

    caam.desc[0] |= HDR_JD_DESCLEN(dc);
    caam_shedule_job_async();

    if (!caam.async_hashing)
        return caam_hash_join();

    return PB_OK;
}

static int caam_hash_input(const void *buf, size_t length, bool async)
{
    size_t bytes_to_process = length;
    uint8_t *buf_p = (uint8_t *)buf;
    size_t bytes_to_copy;
    size_t no_of_blocks;
    int rc;

    /* Every call waits for the job of the one before it, also one that only
     * fills the buffer below (crypto.md 3.3) */
    rc = caam_hash_join();

    if (rc != PB_OK)
        return rc;

    /* The update's kind is written after the join (F-081) */
    caam.async_hashing = async;

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

            /* The buffer is filled again below, and the engine reads it
             * until its job is done (drivers.md 4.2): one block, not worth
             * the overlap */
            rc = caam_hash_join();

            if (rc != PB_OK)
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
    return caam_hash_input(buf, length, false);
}

static int caam_hash_update_async(const void *buf, size_t length)
{
    return caam_hash_input(buf, length, true);
}

static int caam_hash_final(uint8_t *output, size_t size)
{
    int err;
    int dc = 0;

    err = caam_hash_join();

    if (err != PB_OK)
        return err;

    /* Less room than the digest: nothing is written (crypto.md 3.5) */
    if (size < caam.digest_size)
        return -PB_ERR_BUF_TOO_SMALL;

    /* If there was no update we must still call the update function
     * once to initialize the hash block */
    if (caam.init) {
        err = _hash_update(NULL, 0);

        if (err != PB_OK)
            return err;
    }

    /* That job, when the hash's updates were asynchronous */
    err = caam_hash_join();

    if (err != PB_OK)
        return err;

    arch_clean_cache_range((uintptr_t)caam_digest, sizeof(caam_digest));
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
    caam.desc[dc++] = (uint32_t)(uintptr_t)caam_digest;

    caam.desc[0] |= HDR_JD_DESCLEN(dc);

    err = caam_shedule_job_sync();

    arch_invalidate_cache_range((uintptr_t)caam_digest, sizeof(caam_digest));

    caam.hash_align_buf_len = 0;

    /* The digest only, and only of a job that ended well */
    if (err == PB_OK)
        memcpy(output, caam_digest, caam.digest_size);

    return err;
}

/* Program the TRNG's entropy delay and statistical test limits, the values
 * NXP uses in its OP-TEE driver, and clear the TRNG's error bit. */
static void caam_rng_kick(uintptr_t ctrl_base, uint32_t ent_delay)
{
    /* Program mode. Setting ACC too invalidates the entropy register and
     * makes the TRNG generate new entropy. */
    mmio_setbits_32(ctrl_base + CAAM_RTMCTL, CAAM_RTMCTL_PRGM | CAAM_RTMCTL_ACC);

    /* A delay shorter than one already programmed is no use. RTSDCTL reads
     * zero outside program mode. */
    ent_delay = MAX(ent_delay, CAAM_RTSDCTL_DLY_GET(mmio_read_32(ctrl_base + CAAM_RTSDCTL)));

    mmio_write_32(ctrl_base + CAAM_RTSDCTL,
                  CAAM_RTSDCTL_ENT_DLY(ent_delay) | CAAM_RTSDCTL_SAMP_SIZE);
    mmio_write_32(ctrl_base + CAAM_RTFRQMIN, ent_delay >> 2);
    mmio_write_32(ctrl_base + CAAM_RTFRQMAX, ent_delay << 4);
    mmio_write_32(ctrl_base + CAAM_RTSCMISC, CAAM_RTSCMISC_RTY(2) | CAAM_RTSCMISC_LRUN(32));
    mmio_write_32(ctrl_base + CAAM_RTPKRRNG, 570);
    mmio_write_32(ctrl_base + CAAM_RTPKRMAX, 1600);
    mmio_write_32(ctrl_base + CAAM_RTSCML, CAAM_RTSC_LIM(122, 317));
    mmio_write_32(ctrl_base + CAAM_RTSCR1L, CAAM_RTSC_LIM(80, 107));
    mmio_write_32(ctrl_base + CAAM_RTSCR2L, CAAM_RTSC_LIM(57, 62));
    mmio_write_32(ctrl_base + CAAM_RTSCR3L, CAAM_RTSC_LIM(39, 39));
    mmio_write_32(ctrl_base + CAAM_RTSCR4L, CAAM_RTSC_LIM(27, 26));
    mmio_write_32(ctrl_base + CAAM_RTSCR5L, CAAM_RTSC_LIM(19, 18));
    mmio_write_32(ctrl_base + CAAM_RTSCR6PL, CAAM_RTSC_LIM(18, 17));

    /* Run mode, raw sampling */
    mmio_clrsetbits_32(ctrl_base + CAAM_RTMCTL,
                       CAAM_RTMCTL_PRGM | CAAM_RTMCTL_ACC | CAAM_RTMCTL_SAMP_MODE_MASK,
                       CAAM_RTMCTL_SAMP_MODE_RAW);

    /* The error bit is set if the TRNG clock was out of range, e.g. before
     * the PLLs were set up. Write 1 to clear. */
    mmio_setbits_32(ctrl_base + CAAM_RTMCTL, CAAM_RTMCTL_ERR);
}

/* One job for state handle sh: tear it down if it was instantiated without
 * prediction resistance, instantiate it with, and for state handle 0 generate
 * the secure keys if they aren't. */
static int caam_rng_instantiate_sh(uint32_t sh, uint32_t rdsta)
{
    int dc = 1;
    int rc;

    /* The descriptor is a job's until the job is done: one of the state
     * handle before that timed out is waited for, and when it still doesn't
     * end nothing is added behind it */
    rc = caam_idle();

    if (rc != PB_OK)
        return rc;

    if (rdsta & CAAM_RDSTA_IF(sh)) {
        caam.desc[dc++] = RNG_SH_UNINST(sh);
        /* Wait for class 1 done, then clear it for the next operation */
        caam.desc[dc++] = JUMP_C1_LOCAL(TST_ALL_COND_TRUE, JUMP_TST_COND(NONE), 1);
        caam.desc[dc++] = LD_NOCLASS_IMM(REG_CLEAR_WRITTEN, sizeof(uint32_t));
        caam.desc[dc++] = 1;
    }

    caam.desc[dc++] = RNG_SH_INST_PR(sh);

    if (sh == 0 && !(rdsta & CAAM_RDSTA_SKVN)) {
        caam.desc[dc++] = JUMP_C1_LOCAL(TST_ALL_COND_TRUE, JUMP_TST_COND(NONE), 1);
        caam.desc[dc++] = LD_NOCLASS_IMM(REG_CLEAR_WRITTEN, sizeof(uint32_t));
        caam.desc[dc++] = 1;
        caam.desc[dc++] = RNG_GEN_SECKEYS;
    }

    caam.desc[0] = DESC_HEADER(dc);

    return caam_shedule_job_sync();
}

int imx_caam_rng_init(uintptr_t ctrl_base)
{
    int rc = PB_OK;
    uint32_t rng_vid;

    if (!caam.caam_initialized)
        return -PB_ERR_STATE;

    uint32_t era = CAAM_CCBVID_CAAM_ERA(mmio_read_32(caam.base + CAAM_CCBVID));

    if (era < 10)
        rng_vid = CAAM_CHAVID_LS_RNGVID(mmio_read_32(caam.base + CAAM_CHAVID_LS));
    else
        rng_vid = CAAM_RNG_VERSION_VID(mmio_read_32(caam.base + CAAM_RNG_VERSION));

    /* Before RNG4 there are no state handles to instantiate */
    if (rng_vid < 4) {
        LOG_INFO("RNG version %" PRIu32 ", nothing to instantiate", rng_vid);
        return PB_OK;
    }

    uint32_t nr_sh = CAAM_CTPR_MS_RNG_I(mmio_read_32(caam.base + CAAM_CTPR_MS));

    if (nr_sh == 0 || nr_sh > CAAM_RNG_SH_MAX) {
        LOG_ERR("Unexpected number of RNG state handles: %" PRIu32, nr_sh);
        return -PB_ERR_NOT_SUPPORTED;
    }

    uint32_t rdsta_start = mmio_read_32(ctrl_base + CAAM_RDSTA);
    uint32_t ent_delay = CAAM_RNG_ENT_DLY_MIN;
    uint32_t rdsta = rdsta_start;

    for (uint32_t sh = 0; sh < nr_sh; sh++) {
        const uint32_t done = CAAM_RDSTA_IF(sh) | CAAM_RDSTA_PR(sh);

        while ((rdsta & done) != done) {
            /* The TRNG is only programmed while no state handle is
             * instantiated; it can't be changed under one. */
            if ((rdsta & (CAAM_RDSTA_IF(0) | CAAM_RDSTA_IF(1))) == 0)
                caam_rng_kick(ctrl_base, ent_delay);

            rc = caam_rng_instantiate_sh(sh, rdsta);
            rdsta = mmio_read_32(ctrl_base + CAAM_RDSTA);

            if ((rdsta & done) == done)
                break;

            /* A job timeout won't get better with more entropy */
            if (rc == -PB_ERR_TIMEOUT)
                goto err_out;

            /* Not enough entropy: try again with a longer delay */
            ent_delay += CAAM_RNG_ENT_DLY_STEP;

            if (ent_delay > CAAM_RNG_ENT_DLY_MAX) {
                rc = -PB_ERR;
                goto err_out;
            }
        }
    }

    LOG_INFO("RNG4: %" PRIu32 " state handles, RDSTA %08" PRIx32 " -> %08" PRIx32,
             nr_sh,
             rdsta_start,
             rdsta);
    return PB_OK;

err_out:
    LOG_ERR("RNG4 instantiation failed (%i), RDSTA %08" PRIx32, rc, rdsta);
    return rc;
}

int imx_caam_stop(void)
{
    int rc;

    /* Before init, or a second time: nothing to stop */
    if (!caam.caam_initialized)
        return PB_OK;

    /* A job that was given up is waited for: the rings' base and size are
     * only written with no job in them (SRM: JRINTR's error types) */
    rc = caam_idle();

    if (rc != PB_OK)
        return rc;

    /* The rings name pb's memory no more (drivers.md 2.3, crypto.md 6) */
    mmio_write_32(caam.base + CAAM_IRBAR_JR_LS, 0);
    mmio_write_32(caam.base + CAAM_ORBAR_JR_LS, 0);
    mmio_write_32(caam.base + CAAM_IRSR_JR, 0);
    mmio_write_32(caam.base + CAAM_ORSR_JR, 0);

    caam.caam_initialized = false;
    return PB_OK;
}

int imx_caam_init(uintptr_t base)
{
    int rc;

    memset(&caam, 0, sizeof(caam));
    caam.base = base;

    /* The rings, the descriptor and every buffer of the driver's are in
     * these three, and the engine takes 32-bit addresses (drivers.md 4.2) */
    uint32_t addr;

    if (caam_addr(&caam, sizeof(caam), &addr) != PB_OK ||
        caam_addr(&caam_ring, sizeof(caam_ring), &addr) != PB_OK ||
        caam_addr(caam_digest, sizeof(caam_digest), &addr) != PB_OK)
        return -PB_ERR_PARAM;

    uint32_t caam_version = mmio_read_32(caam.base + CAAM_CAAMVID_MS);
    caam.id = CAAM_CAAMVID_MS_IP_ID(caam_version);
    caam.ver_maj = CAAM_CAAMVID_MS_MAJ_REV(caam_version);
    caam.ver_min = CAAM_CAAMVID_MS_MIN_REV(caam_version);

    LOG_INFO("Base 0x%" PRIxPTR " ID: 0x%04x, ver %i.%i",
             caam.base,
             caam.id,
             caam.ver_maj,
             caam.ver_min);

    arch_clean_cache_range((uintptr_t)&caam_ring.input, sizeof(caam_ring.input));
    arch_clean_cache_range((uintptr_t)&caam_ring.output, sizeof(caam_ring.output));

    /* What an earlier user of the job ring left in its output ring (the ROM,
     * a stage before pb) is taken out first: a ring's base and size are only
     * written with no job in the ring (SRM: ORSFR_JRa, ORJRR_JRa, and the
     * error types of JRINTR_JRa). A job of that user's that is still in the
     * engine completes into the ring below, and caam_wait_for_job() tells it
     * by its descriptor */
    uint32_t left = mmio_read_32(caam.base + CAAM_ORSFR_JR) & CAAM_ORSFR_ORSF_MASK;

    if (left != 0) {
        LOG_INFO("%" PRIu32 " completion(s) left in the output ring", left);
        mmio_write_32(caam.base + CAAM_ORJRR_JR, left);
    }

    /* Initialize job rings, one entry each */
    mmio_write_32(caam.base + CAAM_IRBAR_JR_LS, (uint32_t)(uintptr_t)&caam_ring.input);
    mmio_write_32(caam.base + CAAM_ORBAR_JR_LS, (uint32_t)(uintptr_t)&caam_ring.output);
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
