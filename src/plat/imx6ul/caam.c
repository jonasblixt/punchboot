/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <plat.h>
#include <io.h>
#include <tinyprintf.h>
#include <pb_string.h>
#include "caam.h"

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
#define CAAM_ALG_TYPE_SHA256    (0x43 << 16)
#define CAAM_ALG_STATE_UPDATE   (0x00 << 2)
#define CAAM_ALG_STATE_INIT     (0x01 << 2)
#define CAAM_ALG_STATE_FIN      (0x02 << 2)
#define CAAM_ALG_STATE_INIT_FIN (0x03 << 2)
#define CAAM_ALG_AAI(x)         (x << 4)



static struct caam_hash_ctx ctx;
static struct fsl_caam *d;

static int caam_shedule_job_sync(struct fsl_caam *d, uint32_t *job) {

    d->input_ring[0] = (uint32_t) job;
    pb_writel(1, d->base + CAAM_IRJAR0);

    while (pb_readl(d->base + CAAM_ORSFR0) != 1)
        asm("nop");

    if (d->output_ring[0] != (uint32_t) job) {
        tfp_printf ("Job failed\n\r");
        return PB_ERR;
    }
    pb_writel(1, d->base + CAAM_ORJRR0);
    return PB_OK;
}

uint32_t plat_sha256_init(void) {
    memset(&ctx, 0, sizeof(struct caam_hash_ctx));
    return PB_OK;
}

uint32_t plat_sha256_update(uint8_t *data, uint32_t sz) {

    struct caam_sg_entry *e = &ctx.sg_tbl[ctx.sg_count];

    e->addr_lo = (uint32_t) data;
    e->len_flag = sz;

    ctx.sg_count++;
    ctx.total_bytes += sz;
    if (ctx.sg_count > 32) 
        return PB_ERR;
    
    return PB_OK;
}

uint32_t plat_sha256_finalize(uint8_t *out) {
    uint32_t __a4k desc[9];
    struct caam_sg_entry *e_last = &ctx.sg_tbl[ctx.sg_count-1];

    e_last->len_flag |= SG_ENTRY_FINAL_BIT;  
   
    desc[0] = CAAM_CMD_HEADER | 7;
    desc[1] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | CAAM_ALG_TYPE_SHA256 |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_INIT_FIN;

    desc[2] = CAAM_CMD_FIFOL | ( 2 << 25) | (1 << 22) | (0x14 << 16) |(1 << 24);
    desc[3] = (uint32_t) ctx.sg_tbl;
    desc[4] = ctx.total_bytes;

    desc[5] = CAAM_CMD_STORE | (2 << 25) | (0x20 << 16) | 32;
    desc[6] = (uint32_t) out;



    if (caam_shedule_job_sync(d, desc) != PB_OK) {
        tfp_printf ("sha256 error\n\r");
        return PB_ERR;
    }

   return PB_OK;
}

uint32_t plat_rsa_enc(uint8_t *input,  uint32_t input_sz,
                    uint8_t *output,
                    uint8_t *pk_mod, uint32_t key_mod_sz,
                    uint8_t *pk_exp, uint32_t key_exp_sz) {


    uint32_t __a4k desc[10];

   
    desc[0] = CAAM_CMD_HEADER | (7 << 16) | 8;
    desc[1] = (key_exp_sz << 12)|key_mod_sz;
    desc[2] = (uint32_t) input;
    desc[3] = (uint32_t) output;
    desc[4] = (uint32_t) pk_mod;
    desc[5] = (uint32_t) pk_exp;
    desc[6] = input_sz;
    desc[7] = CAAM_CMD_OP | (0x18 << 16)|(0<<12) ;


 
    if (caam_shedule_job_sync(d, desc) != PB_OK) {
        tfp_printf ("caam_rsa_enc error \n\r");
        return PB_ERR;
    }
#ifdef CAAM_DEBUG
    tfp_printf ("0x%8.8X\n\r",pb_readl(d->base+0x8810));
    tfp_printf ("0x%8.8X\n\r",pb_readl(d->base+0x8814));
    tfp_printf ("0x%8.8X\n\r",pb_readl(d->base+0x1044));
#endif

    /* TODO: Implement logging info,error, debug*/


    return PB_OK;

}



void caam_sha256_sum(struct fsl_caam *d, uint8_t* data, uint32_t sz, uint8_t *out) {
    uint32_t __a4k desc[9];

    desc[0] = CAAM_CMD_HEADER | 7;
    desc[1] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | CAAM_ALG_TYPE_SHA256 |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_INIT_FIN;

    desc[2] = CAAM_CMD_FIFOL | ( 2 << 25) | (1 << 22) | (0x14 << 16);
    desc[3] = (uint32_t) data;
    desc[4] = sz;

    desc[5] = CAAM_CMD_STORE | (2 << 25) | (0x20 << 16) | 32;
    desc[6] = (uint32_t) out;



    if (caam_shedule_job_sync(d, desc) != PB_OK) {
        tfp_printf ("sha256 error\n\r");
    }

}


int caam_init(struct fsl_caam *caam_dev) {

    d = caam_dev;

    if (d->base == 0)
        return PB_ERR;

    d->input_ring[0] = 0;
    d->output_ring[0] = 0;

    /* Initialize job rings */
    pb_writel( (uint32_t) d->input_ring,  d->base + CAAM_IRBAR0);
    pb_writel( (uint32_t) d->output_ring, d->base + CAAM_ORBAR0);

    pb_writel( JOB_RING_ENTRIES, d->base + CAAM_IRSR0);
    pb_writel( JOB_RING_ENTRIES, d->base + CAAM_ORSR0);

    return PB_OK;
}
