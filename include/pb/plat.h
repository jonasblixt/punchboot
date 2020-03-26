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

#include <pb/pb.h>
#include <pb/usb.h>
#include <pb/fuse.h>
#include <pb/params.h>
#include <pb/crypto.h>
#include <pb/storage.h>
#include <pb/transport.h>
#include <bpak/keystore.h>

#define PLAT_EMMC_PART_BOOT0 1
#define PLAT_EMMC_PART_BOOT1 2
#define PLAT_EMMC_PART_USER  0

/* Platform API Calls */
void      plat_reset(void);
uint32_t  plat_get_us_tick(void);
void      plat_delay_ms(uint32_t);
void      plat_wdog_init(void);
void      plat_wdog_kick(void);

int plat_early_init(struct pb_storage *storage,
                    struct pb_transport *transport);

void      plat_preboot_cleanup(void);
bool      plat_force_recovery(void);
uint32_t  plat_prepare_recovery(void);
uint32_t  plat_get_params(struct param **pp);
uint32_t  plat_get_uuid(char *out);

/* Crypto Interface */
uint32_t  plat_hash_init(uint32_t hash_kind);
uint32_t  plat_hash_update(uintptr_t bfr, uint32_t sz);
uint32_t  plat_hash_finalize(uintptr_t data, uint32_t sz, uintptr_t out,
                                uint32_t out_sz);

uint32_t  plat_verify_signature(uint8_t *sig, uint32_t sig_kind,
                                uint8_t *hash, uint32_t hash_kind,
                                struct bpak_key *k);

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
