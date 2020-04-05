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
#include <pb/crypto.h>
#include <pb/storage.h>
#include <pb/transport.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/command.h>
#include <pb/boot.h>
#include <bpak/keystore.h>
#include <uuid/uuid.h>

/* Platform API Calls */
void plat_reset(void);
unsigned int plat_get_us_tick(void);
void plat_delay_ms(uint32_t);
void plat_wdog_init(void);
void plat_wdog_kick(void);

int plat_early_init(struct pb_storage *storage,
                    struct pb_transport *transport,
                    struct pb_console *console,
                    struct pb_crypto *crypto,
                    struct pb_command_context *command_ctx,
                    struct pb_boot_context *boot);

void plat_preboot_cleanup(void);
bool plat_force_recovery(void);
int plat_get_uuid(struct pb_crypto *crypto, char *out);

/* FUSE Interface */
uint32_t  plat_fuse_read(struct fuse *f);
uint32_t  plat_fuse_write(struct fuse *f);
uint32_t  plat_fuse_to_string(struct fuse *f, char *s, uint32_t n);

/* Secure boot */
uint32_t plat_setup_device(void);
uint32_t plat_setup_lock(void);
uint32_t plat_get_security_state(uint32_t *state);

#endif  // INCLUDE_PB_PLAT_H_
