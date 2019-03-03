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
#include <crypto.h>

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


struct rsa_enc_pdb 
{
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

struct fsl_caam_jr 
{
    __iomem base;
   uint32_t __a4k input_ring[JOB_RING_ENTRIES];
   uint32_t __a4k output_ring[JOB_RING_ENTRIES*2];
};

uint32_t caam_init(struct fsl_caam_jr *d);

#endif /* __CAAM_H__ */
