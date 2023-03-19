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
#include <bpak/id.h>

/* PB Primary boot state partition */
#define UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e (const unsigned char *) "\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
/* PB Backup boot state partition */
#define UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee (const unsigned char *) "\x65\x6a\xb3\xfc\x58\x56\x4a\x5e\xa2\xae\x5a\x01\x83\x13\xb3\xee"

#define PB_STATE_MAGIC 0x026d4a65

#define PB_STATE_A_ENABLED (1 << 0)
#define PB_STATE_B_ENABLED (1 << 1)
#define PB_STATE_A_VERIFIED (1 << 0)
#define PB_STATE_B_VERIFIED (1 << 1)
#define PB_STATE_ERROR_A_ROLLBACK (1 << 0)
#define PB_STATE_ERROR_B_ROLLBACK (1 << 1)

struct pb_boot_state /* 512 bytes */
{
    uint32_t magic;                     /*!< PB boot state magic number */
    uint32_t enable;                    /*!< Boot partition enable bits */
    uint32_t verified;                  /*!< Boot partition verified bits */
    uint32_t remaining_boot_attempts;   /*!< Rollback boot counter */
    uint32_t error;                     /*!< Rollback error bits */
    uint8_t rz[488];                    /*!< Reserved, set to zero */
    uint32_t crc;                       /*!< State checksum */
} __attribute__((packed));

/** Boot modes */
enum pb_boot_mode {
    PB_BOOT_MODE_INVALID = 0,  /*!< Invalid boot mode */
    PB_BOOT_MODE_NORMAL,       /*!< Normal boot mode */
    PB_BOOT_MODE_CMD,          /*!< Boot was initiated through the command mode interface */
};

enum pb_boot_source {
    PB_BOOT_SOURCE_INVALID = 0,
    PB_BOOT_SOURCE_BLOCK_DEV,
    PB_BOOT_SOURCE_TRANSPORT,
};

typedef int (*pb_board_early_boot_cb_t) (void *plat);
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
    bpak_id_t image_bpak_id;                 /*!< BPAK id of bootable part */
    bpak_id_t dtb_bpak_id;                   /*!< Optional BPAK id of a device tree part */
    bpak_id_t ramdisk_bpak_id;               /*!< Optional BPAK id of a ramdisk part */
    enum pb_rollback_mode rollback_mode;    /*!< Rollback mode configuration */
    pb_board_early_boot_cb_t early_boot_cb; /*!< Optional early boot callback */
    pb_board_late_boot_cb_t late_boot_cb;   /*!< Optional late boot callback */
    pb_board_patch_dtb_cb_t dtb_patch_cb;   /*!< Optional DTB patch callback */
    bool set_dtb_boot_arg;                  /*!< Pass dtb address as first argument */
    bool print_time_measurements;           /*!< Print internal time measurements
                                                 just before jumping to the boot image */
};

/**
 * Initializes the boot module
 *
 * @return PB_OK on success or a negative number
 */
int pb_boot_init(void);

/**
 * Loads a bootable BPAK image into ram
 *
 * Boot sources:
 *   PB_BOOT_SOURCE_BLOCK_DEV: The boot process tries to load an image
 *   from an active boot partition
 *
 *   PB_BOOT_SOURCE_TRANSPORT: The boot process tries to load an image into
 *   ram using the command mode transport
 *
 * @param[in] boot_source Reads from block device in block dev mode or
 *                      over command mode transport in transport mode.
 * @param[in] boot_part_uu Optional partition UUID, if this is not NULL
 *                          it will override the boot state. This is only
 *                          relevant in normal mode
 *
 * @return PB_OK on success or a negative number
 */
int pb_boot_load(enum pb_boot_source boot_source, uuid_t boot_part_uu);

/**
 * Boot the system. This function will also update the device tree
 * with, for example, ramdisk size, active boot partition etc.
 *
 * @param[in] verbose Sets verbose boot mode
 * @param[in] boot_mode Boot mode
 *
 * @return PB_OK on success or a negative number
 */
int pb_boot(enum pb_boot_mode boot_mode, bool verbose);

/**
 * Activate a boot partition
 *
 * @param[in] uu UUID of partition to activate
 *
 * @return PB_OK on success or a negative number
 */
int pb_boot_activate(uuid_t uu);

/**
 * Read active boot partition
 *
 * @param[out] out UUID of active boot partition
 *
 * @return PB_OK on success or a negative number
 */
int pb_boot_read_active_part(uuid_t out);

#endif  // INCLUDE_PB_BOOT_H
