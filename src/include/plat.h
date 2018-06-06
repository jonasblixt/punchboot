/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __PLAT_H__
#define __PLAT_H__

#include <pb.h>

#define PLAT_EMMC_PART_BOOT0 1
#define PLAT_EMMC_PART_BOOT1 2
#define PLAT_EMMC_PART_USER  0

typedef uint32_t t_plat_usb_cb(uint8_t*, uint8_t*, uint8_t*);

extern const uint8_t part_type_system_a[];
extern const uint8_t part_type_system_b[];
extern const uint8_t part_type_config[];

/* Platform API Calls */
void plat_reset(void);
uint32_t  plat_get_ms_tick(void);

/* EMMC Interface */
uint32_t  plat_emmc_write_block(uint32_t lba_offset, uint8_t *bfr, uint32_t no_of_blocks);
uint32_t  plat_emmc_read_block(uint32_t lba_offset, uint8_t *bfr, uint32_t no_of_blocks);
uint32_t  plat_emmc_switch_part(uint8_t part_no);
uint64_t  plat_emmc_get_lastlba(void);

/* Crypto Interface */
uint32_t  plat_sha256_init(void);
uint32_t  plat_sha256_update(uint8_t *bfr, uint32_t sz);
uint32_t  plat_sha256_finalize(uint8_t *out);
uint32_t  plat_rsa_enc(uint8_t *sig, uint32_t sig_sz, uint8_t *out, 
                    uint8_t *pk_modulus, uint32_t pk_modulus_sz,
                    uint8_t *pk_exponent, uint32_t pk_exponent_sz);

/* USB Interface */
uint32_t  plat_usb_cmd_callback(t_plat_usb_cb *cb);
uint32_t  plat_usb_send(uint8_t *bfr, uint32_t sz);
uint32_t  plat_usb_prep_bulk_buffer(uint16_t no_of_blocks, uint8_t buffer_id);
void plat_usb_task(void);

/* UART Interface */
void plat_uart_putc(void *ptr, char c);


#endif
