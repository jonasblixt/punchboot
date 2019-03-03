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
#include <string.h>
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

static uint32_t caam_shedule_job_sync(struct fsl_caam_jr *d, uint32_t *job) 
{

    d->input_ring[0] = (uint32_t)(uintptr_t) job;

    if (d == NULL)
        return PB_ERR;

    pb_write32(1, d->base + CAAM_IRJAR);

    while ((pb_read32(d->base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    if (d->output_ring[0] != (uint32_t)(uintptr_t) job) 
    {
        printf ("Job failed\n\r");
        return PB_ERR;
    }
    pb_write32(1, d->base + CAAM_ORJRR);
    return PB_OK;
}


static uint32_t caam_shedule_job_async(struct fsl_caam_jr *d, uint32_t *job) 
{

    d->input_ring[0] = (uint32_t)(uintptr_t) job;

    if (d == NULL)
        return PB_ERR;

    pb_write32(1, d->base + CAAM_IRJAR);

    return PB_OK;
}

static uint32_t caam_wait_for_job(struct fsl_caam_jr *d, uint32_t *job)
{
    while ((pb_read32(d->base + CAAM_ORSFR) & 1) == 0)
        __asm__("nop");

    if (d->output_ring[0] != (uint32_t)(uintptr_t) job) 
    {
        printf ("Job failed\n\r");
        return PB_ERR;
    }
    pb_write32(1, d->base + CAAM_ORJRR);
    return PB_OK;
}
/*
uint32_t caam_sha256_init(void) 
{
    memset(&ctx, 0, sizeof(struct caam_hash_ctx));
    return PB_OK;
}

uint32_t caam_sha256_update(uint8_t *data, uint32_t sz) 
{

    struct caam_sg_entry *e = &ctx.sg_tbl[ctx.sg_count];


    LOG_DBG("SHA256 update %p sz %u sgcount %u",data,sz, ctx.sg_count);
    e->addr_lo = (uint32_t)(uintptr_t) data;
    e->len_flag = sz;

    ctx.sg_count++;
    ctx.total_bytes += sz;
    if (ctx.sg_count > 32) 
        return PB_ERR;
    
    return PB_OK;
}

uint32_t caam_sha256_finalize(uint8_t *out) 
{
    struct caam_sg_entry *e_last = &ctx.sg_tbl[ctx.sg_count-1];

    e_last->len_flag |= SG_ENTRY_FINAL_BIT;  
   
    desc[0] = CAAM_CMD_HEADER | 7;
    desc[1] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | CAAM_ALG_TYPE_SHA256 |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_INIT_FIN;

    desc[2] = CAAM_CMD_FIFOL | ( 2 << 25) | (1 << 22) | (0x14 << 16) |(1 << 24);
    desc[3] = (uint32_t)(uintptr_t) ctx.sg_tbl;
    desc[4] = ctx.total_bytes;

    desc[5] = CAAM_CMD_STORE | (2 << 25) | (0x20 << 16) | 32;
    desc[6] = (uint32_t)(uintptr_t) out;


    LOG_DBG("Finalize");
    if (caam_shedule_job_sync(d, desc) != PB_OK) 
    {
        LOG_ERR ("sha256 error");
        return PB_ERR;
    }

   return PB_OK;
}
*/

static volatile __a4k __no_bss hash_ctx[128];
uint32_t caam_sha256_init(void) 

{
    memset(hash_ctx, 0, 128);
    memset(&ctx, 0, sizeof(struct caam_hash_ctx));
    return PB_OK;
}


uint32_t caam_sha256_update(uint8_t *data, uint32_t sz) 
{
    uint8_t dc = 0;

    if (ctx.sg_count)
        caam_wait_for_job(d, desc);

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | CAAM_ALG_TYPE_SHA256 |
        CAAM_ALG_AAI(0);


    if (ctx.sg_count == 0)
    {
        desc[1] |= CAAM_ALG_STATE_INIT;
        LOG_INFO("Init %p, %u",data,sz);
    }
    else
    {
        desc[1] |= CAAM_ALG_STATE_UPDATE;
        desc[dc++]  = LD_NOIMM(CLASS_2, REG_CTX,64);
        desc[dc++]  = (uint32_t) (uintptr_t) hash_ctx;
        LOG_INFO("Update %p, %u",data,sz);
    }

    ctx.sg_count = 1;

    desc[dc++] = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    desc[dc++] = (uint32_t) (uintptr_t) data;
    desc[dc++] = sz;
    desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, 64);
    desc[dc++] = (uint32_t) (uintptr_t) hash_ctx;

    desc[0] |= dc;

    if (caam_shedule_job_async(d, desc) != PB_OK) 
    {
        LOG_ERR ("sha256 error");
        return PB_ERR;
    }
    return PB_OK;
}

uint32_t caam_sha256_finalize(uint8_t *out) 
{
    uint8_t dc = 0;

    caam_wait_for_job(d, desc);

    desc[dc++] = CAAM_CMD_HEADER;
    desc[dc++] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | CAAM_ALG_TYPE_SHA256 |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_FIN;

    desc[dc++]  = LD_NOIMM(CLASS_2, REG_CTX,64);
    desc[dc++]  = (uint32_t) (uintptr_t) hash_ctx;
    desc[dc++]  = FIFO_LD_EXT(CLASS_2, MSG, LAST_C2);
    desc[dc++]  = 0;
    desc[dc++]  = 0;
    desc[dc++] = ST_NOIMM(CLASS_2, REG_CTX, 64);
    desc[dc++] = (uint32_t) (uintptr_t) out;

    desc[0] |= dc;

    LOG_DBG("Finalize");
    if (caam_shedule_job_sync(d, desc) != PB_OK) 
    {
        LOG_ERR ("sha256 error");
        return PB_ERR;
    }

    printf("SHA:");
    for (uint8_t n = 0; n < 32; n++)
        printf ("%02x ",out[n]);
    printf("\n\r");

   return PB_OK;
}

uint32_t caam_md5_init(void) 
{
    memset(&ctx, 0, sizeof(struct caam_hash_ctx));
    return PB_OK;
}

uint32_t caam_md5_update(uint8_t *data, uint32_t sz) 
{

    struct caam_sg_entry *e = &ctx.sg_tbl[ctx.sg_count];


    LOG_DBG("MD5 update %p sz %u sgcount %u",data,sz, ctx.sg_count);
    e->addr_lo = (uint32_t)(uintptr_t) data;
    e->len_flag = sz;

    ctx.sg_count++;
    ctx.total_bytes += sz;
    if (ctx.sg_count > 32) 
        return PB_ERR;
    
    return PB_OK;
}

uint32_t caam_md5_finalize(uint8_t *out) 
{
    struct caam_sg_entry *e_last = &ctx.sg_tbl[ctx.sg_count-1];

    e_last->len_flag |= SG_ENTRY_FINAL_BIT;  
   
    desc[0] = CAAM_CMD_HEADER | 7;
    desc[1] = CAAM_CMD_OP | CAAM_OP_ALG_CLASS2 | CAAM_ALG_TYPE_MD5 |
        CAAM_ALG_AAI(0) | CAAM_ALG_STATE_INIT_FIN;

    desc[2] = CAAM_CMD_FIFOL | ( 2 << 25) | (1 << 22) | (0x14 << 16) |(1 << 24);
    desc[3] = (uint32_t)(uintptr_t) ctx.sg_tbl;
    desc[4] = ctx.total_bytes;

    desc[5] = CAAM_CMD_STORE | (2 << 25) | (0x20 << 16) | 16;
    desc[6] = (uint32_t)(uintptr_t) out;


    LOG_DBG("Finalize");
    if (caam_shedule_job_sync(d, desc) != PB_OK) 
        return PB_ERR;

   return PB_OK;
}


uint32_t caam_rsa_enc(uint8_t *input,  uint32_t input_sz,
                    uint8_t *output, struct asn1_key *k)
{
   
    desc[0] = CAAM_CMD_HEADER | (7 << 16) | 8;
    desc[1] = (3 << 12)|512;
    desc[2] = (uint32_t)(uintptr_t) input;
    desc[3] = (uint32_t)(uintptr_t) output;
    desc[4] = (uint32_t)(uintptr_t) k->mod;
    desc[5] = (uint32_t)(uintptr_t) k->exp;
    desc[6] = input_sz;
    desc[7] = CAAM_CMD_OP | (0x18 << 16)|(0<<12) ;


 
    if (caam_shedule_job_sync(d, desc) != PB_OK) {
        LOG_ERR ("caam_rsa_enc error");
        return PB_ERR;
    }

    return PB_OK;
}


/*
 * Operation:
 *   (0x16 << 16) DSA_Verfiy (pg. 335)
 *
 *
 */

uint32_t caam_ecc_enc(uint8_t *input,  uint32_t input_sz,
                    uint8_t *output, struct asn1_key *k)
{
   
    desc[0] = CAAM_CMD_HEADER | (7 << 16) | 8;
    desc[1] = (3 << 12)|512;
    desc[2] = (uint32_t)(uintptr_t) input;
    desc[3] = (uint32_t)(uintptr_t) output;
    desc[4] = (uint32_t)(uintptr_t) k->mod;
    desc[5] = (uint32_t)(uintptr_t) k->exp;
    desc[6] = input_sz;
    desc[7] = CAAM_CMD_OP | (0x18 << 16)|(0<<12) ;


 
    if (caam_shedule_job_sync(d, desc) != PB_OK) {
        LOG_ERR ("caam_rsa_enc error");
        return PB_ERR;
    }

    return PB_OK;
}


uint32_t caam_init(struct fsl_caam_jr *caam_dev) 
{

    d = caam_dev;

    if (d->base == 0)
        return PB_ERR;
    
    for (uint32_t n = 0;n < JOB_RING_ENTRIES; n++)
        d->input_ring[n] = 0;

    for (uint32_t n = 0;n < JOB_RING_ENTRIES*2; n++)
        d->output_ring[n] = 0;

    /* Initialize job rings */
    pb_write32( (uint32_t)(uintptr_t) d->input_ring,  d->base + CAAM_IRBAR);
    pb_write32( (uint32_t)(uintptr_t) d->output_ring, d->base + CAAM_ORBAR);

    pb_write32( JOB_RING_ENTRIES, d->base + CAAM_IRSR);
    pb_write32( JOB_RING_ENTRIES, d->base + CAAM_ORSR);

    return PB_OK;
}

