/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */



#ifndef __CAAM_H__
#define	__CAAM_H__

#include <pb.h>

#define JOB_RING_ENTRIES 1

#define CAAM_MCFGR           0x0004
#define CAAM_SCFGR           0x000c
#define CAAM_JR0MIDR         0x0010
#define CAAM_JR1MIDR         0x0018
#define CAAM_DECORR          0x009c
#define CAAM_DECO0MID        0x00a0
#define CAAM_DAR             0x0120
#define CAAM_DRR             0x0124
#define CAAM_JDKEKR          0x0400
#define CAAM_TDKEKR          0x0420
#define CAAM_TDSKR           0x0440
#define CAAM_SKNR            0x04e0
#define CAAM_SMSTA           0x0FB4
#define CAAM_STA             0x0FD4
#define CAAM_SMPO_0          0x1FBC
#define CAAM_IRBAR0          0x1004
#define CAAM_IRSR0           0x100c
#define CAAM_IRSAR0          0x1014
#define CAAM_IRJAR0          0x101c
#define CAAM_ORBAR0          0x1024
#define CAAM_ORSR0           0x102c
#define CAAM_ORJRR0          0x1034
#define CAAM_ORSFR0          0x103c
#define CAAM_JRSTAR0         0x1044
#define CAAM_JRINTR0         0x104c
#define CAAM_JRCFGR0_MS      0x1050
#define CAAM_JRCFGR0_LS      0x1054
#define CAAM_IRRIR0          0x105c
#define CAAM_ORWIR0          0x1064
#define CAAM_JRCR0           0x106c
#define CAAM_SMCJR0          0x10f4
#define CAAM_SMCSJR0         0x10fc
#define CAAM_SMAPJR0_PRTN1   0x1114
#define CAAM_SMAG2JR0_PRTN1  0x1118
#define CAAM_SMAG1JR0_PRTN1  0x111c
#define CAAM_SMPO            0x1fbc

struct caam_sg_entry {
	uint32_t addr_hi;	/* Memory Address of start of buffer - hi */
	uint32_t addr_lo;	/* Memory Address - lo */

	uint32_t len_flag;	/* Length of the data in the frame */
#define SG_ENTRY_LENGTH_MASK	0x3FFFFFFF
#define SG_ENTRY_EXTENSION_BIT	0x80000000
#define SG_ENTRY_FINAL_BIT	0x40000000
	uint32_t bpid_offset;
#define SG_ENTRY_BPID_MASK	0x00FF0000
#define SG_ENTRY_BPID_SHIFT	16
#define SG_ENTRY_OFFSET_MASK	0x00001FFF
#define SG_ENTRY_OFFSET_SHIFT	0
};


struct rsa_enc_pdb {
    uint32_t header; // 24..13 = e_len, 12..0 = n_len
    uint32_t f_ref; // Input pointer
    uint32_t g_ref; // Output pointer
    uint32_t n_ref; // PK Modulus pointer
    uint32_t e_ref; // PK exponent pointer
    uint32_t f_len; // Input length
} __attribute__ ((packed));

struct caam_hash_ctx 
{
    struct caam_sg_entry sg_tbl[32];
    uint16_t sg_count;
    uint32_t total_bytes;
};

struct fsl_caam {
    __iomem base;
   uint32_t __a4k input_ring[JOB_RING_ENTRIES];
   uint32_t __a4k output_ring[JOB_RING_ENTRIES*2];
};

uint32_t caam_init(struct fsl_caam *d);
uint32_t caam_sha256_init(void);
uint32_t caam_sha256_update(uint8_t *data, uint32_t sz);
uint32_t caam_sha256_finalize(uint8_t *out);
uint32_t caam_rsa_enc(uint8_t *input,  uint32_t input_sz,
                    uint8_t *output, struct asn1_key *k);

#endif /* __CAAM_H__ */
