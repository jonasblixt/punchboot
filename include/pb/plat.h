/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_PLAT_H_
#define INCLUDE_PB_PLAT_H_

#include <pb/pb.h>
#include <pb/boot.h>
#include <pb/fuse.h>
#include <pb/boot.h>
#include <pb/board.h>
#include <pb/fuse.h>
#include <bpak/keystore.h>
#include <pb-tools/wire.h>

/* Platform API Calls */

/**
 * Initialize the platform
 *
 * @return PB_OK on success or a negative number
 **/
int plat_init(void);

/**
 * Platform specific reset function
 **/
void plat_reset(void);

/**
 * Read platform/system tick
 *
 * @return current micro second tick
 **/
unsigned int plat_get_us_tick(void);

/**
 * Kick watchdog
 */
void plat_wdog_kick(void);

/**
 * Returns a platform specific boot reason as an integer.
 *
 * @return >= 0 for valid boot reasons or a negative number on error
 *
 **/
int plat_boot_reason(void);

/**
 * Returns a platform specific boot reason as an string
 *
 * @return Boot reason string or ""
 *
 **/
const char* plat_boot_reason_str(void);

bool plat_force_command_mode(void);
int plat_get_uuid(char *out);
int plat_patch_bootargs(void *fdt, int offset, bool verbose_boot);
int plat_boot_override(uint8_t *uuid);

/**
 * Early boot process call back.
 *
 * This will optionally call a board level callback if it's defined in
 * the boards boot config struct
 *
 * @return PB_OK on success or a negative number, -PB_ERR_ABORT is a special
 * case that can be used by the board code to intentionally stop the boot
 * process.
 */
int plat_early_boot(void);

/**
 * Late boot process call back.
 *
 * This will optionally call a board level callback if it's defined in
 * the boards boot config struct
 *
 * @param[in] boot_mode Indicates if we're booting normally or if the boot
 *                      was initiated by the command mode interface.
 *
 * @return PB_OK on success or a negative number, -PB_ERR_ABORT is a special
 * case that can be used by the board code to intentionally stop the boot
 * process.
 */
int plat_late_boot(uuid_t boot_part_uu, enum pb_boot_mode boot_mode);

int plat_command(uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size);

int plat_status(void *response_bfr,
                    size_t *response_size);

/* Fusebox API */
int plat_fuse_read(struct fuse *f);
int plat_fuse_write(struct fuse *f);
int plat_fuse_to_string(struct fuse *f, char *s, uint32_t n);

/* Security Life Cycle (SLC) API */
int plat_slc_init(void);
int plat_slc_set_configuration(void);
int plat_slc_set_configuration_lock(void);
int plat_slc_set_end_of_life(void);
int plat_slc_read(enum pb_slc *slc);
int plat_slc_key_active(uint32_t id, bool *active);
int plat_slc_revoke_key(uint32_t id);
int plat_slc_get_key_status(struct pb_result_slc_key_status **status);

/* Crypto API */

/**
 * Optional platform initialization call for crypto hardware.
 *
 * @return PB_OK on success or a negative number
 *
 **/
int plat_crypto_init(void);

/**
 * Initialize the hashing context
 *
 * @param[in] alg Hashing algorithm to run
 *
 * @return PB_OK on success or a negative number
 *
 **/
int plat_hash_init(enum pb_hash_algs alg);

/**
 * Update the hashing context with data
 *
 * @param[in] buf Input byte buffer
 * @param[in] len Length of byte buffer
 *
 * @return PB_OK on success or a negative number
 **/
int plat_hash_update(uint8_t *buf, size_t len);

/**
 * Output hash
 *
 * @param[in] output Output byte buffer
 * @param[in] len Length of byte buffer
 *
 * @return PB_OK on success or a negative number
 **/
int plat_hash_output(uint8_t *output, size_t len);

int plat_pk_verify(uint8_t *signature, size_t signature_len,
                   uint8_t *hash, enum pb_hash_algs alg,
                   struct bpak_key *key);

/* Transport API */
int plat_transport_init(void);
int plat_transport_process(void);
bool plat_transport_ready(void);
int plat_transport_write(void *buf, size_t size);
int plat_transport_read(void *buf, size_t size);


#endif  // INCLUDE_PB_PLAT_H_
