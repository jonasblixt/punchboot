/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_CAAM_H_
#define PLAT_IMX_CAAM_H_

#include <pb/pb.h>
#include <pb/crypto.h>

#define JOB_RING_ENTRIES 1


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

struct caam_sg_entry
{
    uint32_t addr_hi;    /* Memory Address of start of buffer - hi */
    uint32_t addr_lo;    /* Memory Address - lo */

    uint32_t len_flag;    /* Length of the data in the frame */
#define SG_ENTRY_LENGTH_MASK    0x3FFFFFFF
#define SG_ENTRY_EXTENSION_BIT    0x80000000
#define SG_ENTRY_FINAL_BIT    0x40000000
    uint32_t bpid_offset;
#define SG_ENTRY_BPID_MASK    0x00FF0000
#define SG_ENTRY_BPID_SHIFT    16
#define SG_ENTRY_OFFSET_MASK    0x00001FFF
#define SG_ENTRY_OFFSET_SHIFT    0
};


struct rsa_enc_pdb
{
    uint32_t header;  // 24..13 = e_len, 12..0 = n_len
    uint32_t f_ref;  // Input pointer
    uint32_t g_ref;  // Output pointer
    uint32_t n_ref;  // PK Modulus pointer
    uint32_t e_ref;  // PK exponent pointer
    uint32_t f_len;  // Input length
} __attribute__((packed));

struct caam_hash_ctx
{
    struct caam_sg_entry sg_tbl[32];
    uint16_t sg_count;
    uint32_t total_bytes;
};

int imx_caam_init(void);

int caam_pk_verify(struct pb_hash_context *hash,
                    struct bpak_key *key,
                    void *signature, size_t size);

int caam_hash_finalize(struct pb_hash_context *ctx,
                              void *buf, size_t size);

int caam_hash_init(struct pb_hash_context *ctx, enum pb_hash_algs pb_alg);

int caam_hash_update(struct pb_hash_context *ctx, void *buf, size_t size);

#endif  // PLAT_IMX_CAAM_H_
