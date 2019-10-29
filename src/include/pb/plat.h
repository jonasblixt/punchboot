/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef INCLUDE_PB_PLAT_H_
#define INCLUDE_PB_PLAT_H_

#include <pb.h>
#include <usb.h>
#include <fuse.h>
#include <params.h>
#include <crypto.h>

#define PLAT_EMMC_PART_BOOT0 1
#define PLAT_EMMC_PART_BOOT1 2
#define PLAT_EMMC_PART_USER  0

/* Platform API Calls */
void      plat_reset(void);
uint32_t  plat_get_us_tick(void);
void      plat_delay_ms(uint32_t);
void      plat_wdog_init(void);
void      plat_wdog_kick(void);
uint32_t  plat_early_init(void);
void      plat_preboot_cleanup(void);
bool      plat_force_recovery(void);
uint32_t  plat_prepare_recovery(void);
uint32_t  plat_get_params(struct param **pp);
uint32_t  plat_get_uuid(char *out);

/* Block Device Interface */
uint32_t  plat_write_block(uint32_t lba_offset,
                                uintptr_t bfr,
                                uint32_t no_of_blocks);

uint32_t  plat_read_block(uint32_t lba_offset,
                          uintptr_t bfr,
                          uint32_t no_of_blocks);

uint32_t  plat_switch_part(uint8_t part_no);
uint64_t  plat_get_lastlba(void);
uint32_t plat_flush_block(void);

uint32_t plat_write_block_async(uint32_t lba_offset,
                          uintptr_t bfr,
                          uint32_t no_of_blocks);
/* Crypto Interface */
uint32_t  plat_hash_init(uint32_t hash_kind);
uint32_t  plat_hash_update(uintptr_t bfr, uint32_t sz);
uint32_t  plat_hash_finalize(uintptr_t out);

uint32_t  plat_verify_signature(uint8_t *sig, uint32_t sig_kind,
                                uint8_t *hash, uint32_t hash_kind,
                                struct pb_key *k);

/* USB Interface API */
uint32_t  plat_usb_init(struct usb_device *dev);
void      plat_usb_task(struct usb_device *dev);
uint32_t  plat_usb_transfer(struct usb_device *dev, uint8_t ep,
                            uint8_t *bfr, uint32_t sz);
void      plat_usb_set_address(struct usb_device *dev, uint32_t addr);
void      plat_usb_set_configuration(struct usb_device *dev);
void      plat_usb_wait_for_ep_completion(struct usb_device *dev,
                                            uint32_t ep);

/* UART Interface */
void      plat_uart_putc(void *ptr, char c);

/* FUSE Interface */
uint32_t  plat_fuse_read(struct fuse *f);
uint32_t  plat_fuse_write(struct fuse *f);
uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n);

/* Secure boot */
uint32_t plat_setup_device(struct param *params);
uint32_t plat_setup_lock(void);
uint32_t plat_get_security_state(uint32_t *state);

#endif  // INCLUDE_PB_PLAT_H_
