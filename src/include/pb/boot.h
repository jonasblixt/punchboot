/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BOOT_H
#define INCLUDE_PB_BOOT_H

#include <stdint.h>
#include <uuid.h>

#define PB_STATE_MAGIC 0x026d4a65

struct pb_boot_state /* 512 bytes */
{
    uint32_t magic;
    uint8_t private[468];
    uint8_t rz[36];
    uint32_t crc;
} __attribute__((packed));

/** Boot modes */
enum pb_boot_mode {
    PB_BOOT_MODE_INVALID = 0,  /*!< Invalid boot mode */
    PB_BOOT_MODE_NORMAL,       /*!< Normal boot mode */
    PB_BOOT_MODE_CMD,          /*!< Boot was initiated through the command mode interface */
};

typedef int (*pb_board_early_boot_cb_t) (void *plat);

/** The board late boot callback should return 0 under normal circumstances
 * or a negative number, and the special case '-PB_ERR_ABORT' */
typedef int (*pb_board_late_boot_cb_t) (void *plat, uuid_t boot_part_uu, enum pb_boot_mode mode);


typedef int (*pb_board_patch_dtb_cb_t) (void *plat, void *fdt, int offset, bool verbose_boot);

/** Rollback modes */
enum pb_rollback_mode
{
    PB_ROLLBACK_MODE_INVALID = 0, /*!< Invalid rollback mode */
    PB_ROLLBACK_MODE_NORMAL,      /*!< Normal, if we try to rollback to
                                    an inactive/unverified partition, the
                                    rollback will fail */
    PB_ROLLBACK_MODE_SPECULATIVE, /*!< Speculative, if we rollback to an
                                    unverified boot partition the 'boot try counter'
                                    will be reset to '1', this will cause
                                    punchboot to ping/pong between A/B partitions
                                    until a boot is succesful */
};

struct pb_boot_config
{
    const char * a_boot_part_uuid;          /*!< Boot partition A UUID */
    const char * b_boot_part_uuid;          /*!< Boot partition B UUID */
    const char * primary_state_part_uuid;   /*!< Boot partition A UUID */
    const char * backup_state_part_uuid;    /*!< Boot partition B UUID */
    uint32_t image_bpak_id;                 /*!< BPAK id of bootable part */
    uint32_t dtb_bpak_id;                   /*!< Optional BPAK id of a device tree part */
    uint32_t ramdisk_bpak_id;               /*!< Optional BPAK id of a ramdisk part */
    enum pb_rollback_mode rollback_mode;    /*!< Rollback mode configuration */
    pb_board_early_boot_cb_t early_boot_cb; /*!< Optional early boot callback */
    pb_board_late_boot_cb_t late_boot_cb;   /*!< Optional late boot callback */
    pb_board_patch_dtb_cb_t dtb_patch_cb;   /*!< Optional DTB patch callback */
    bool set_dtb_boot_arg;                  /*!< Pass dtb address as first argument */
    bool print_time_measurements;           /*!< Print internal time measurements
                                                 just before jumping to the boot image */
};

int pb_boot_init(void);

int pb_boot_load_state(void);

int pb_boot_load_transport(void);

int pb_boot_load_fs(uint8_t *boot_part_uu);

int pb_boot(bool verbose, enum pb_boot_mode boot_mode);

int pb_boot_activate(uint8_t *uu);
void pb_boot_status(char *status_msg, size_t len);

/* Boot driver API */
int pb_boot_driver_load_state(struct pb_boot_state *state, bool *commit);
uint8_t *pb_boot_driver_get_part_uu(void);
int pb_boot_driver_boot(int *dtb, int offset);
int pb_boot_driver_activate(struct pb_boot_state *state, uint8_t *uu);
int pb_boot_driver_set_part_uu(uint8_t *uu);
void pb_boot_driver_status(struct pb_boot_state *state,
                           char *status_msg, size_t len);

#endif  // INCLUDE_PB_BOOT_H
