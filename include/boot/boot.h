/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_BOOT_H
#define INCLUDE_BOOT_H

#include <pb/utils_def.h>
#include <uuid.h>
#include <bpak/bpak.h>
#include <pb/bio.h>

/* TODO: Write a note about the bpak block size of 512 bytes */
typedef int (*boot_read_cb_t)(int block_offset, size_t length, void *buf);
typedef int (*boot_result_cb_t)(int result);

/** Boot sources */
enum boot_source {
    BOOT_SOURCE_INVALID = 0,  /*!< Invalid boot mode */
    BOOT_SOURCE_BIO,          /*!< Load from block device */
    BOOT_SOURCE_IN_MEM,       /*!< Authenticate and verify a pre-loaded image */
    BOOT_SOURCE_CB,           /*!< Load through callback functions */
    BOOT_SOURCE_END,
};

/**
 * Boot flags
 *
 * Bits 15 - 0 are reserved for punchboot
 * Bits 31 - 16 are intended for board/user
 *
 * \def BOOT_FLAG_CMD
 * Set when boot was initiated through the command mode interface.
 *
 * \def BOOT_FLAG_VERBOSE
 * Set to 1 to request verbose output during boot
 *
 */
#define BOOT_FLAG_CMD     BIT(0)
#define BOOT_FLAG_VERBOSE BIT(1)

struct boot_driver
{
    enum boot_source default_boot_source;
    int (*early_boot_cb)(void);
    bio_dev_t (*get_boot_bio_device)(void);
    int (*set_boot_partition)(uuid_t part_uu);
    void (*get_boot_partition)(uuid_t part_uu);
    int (*get_in_mem_image)(struct bpak_header **header);
    int (*prepare)(struct bpak_header *header, uuid_t boot_part_uu);
    int (*late_boot_cb)(struct bpak_header *header, uuid_t boot_part_uu);
    void (*jump)(void);
};

/**
 * Initializes the boot module
 *
 * @param[in] driver Configuration input
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM on invalid configuration
 */
int boot_init(const struct boot_driver *driver);

/**
 * Configure boot source
 *
 * @param[in] source Boot source
 *
 * @return PB_OK on success
 *        -PB_ERR_PARAM on invalid boot source
 *
 */
int boot_set_source(enum boot_source source);

/**
 * Configure read/result callbacks for boot source: 'BOOT_SOURCE_CB'
 *
 * When the boot source is set to 'BOOT_SOURCE_CB' the boot module
 * will call these functions in the following order
 *
 * 1. read_f(-4096, 4096) To read the BPAK header
 * 2. result_f with the result from the header validation/authenticatoin
 * 3. read_f(<offset>, <chunk sz>) Repetedly to transfer chunks of parts
 * 4. result_f When the chunked transfer of one part is complete
 *
 *  Steps 3 and 4 are repeted until all parts are transfered
 *
 * 5. result_f When payload has been verified
 *
 * @param[in] read_f Read function callback
 * @param[in] result_f Optional result function callback
 *
 */
void boot_configure_load_cb(boot_read_cb_t read_f,
                           boot_result_cb_t result_f);

/**
 * Clear and/or Set flags
 *
 * These bits are available through out the boot flow and can control
 * board specific behaviour.
 *
 * The lower 16 bit's are reserved for punchboot and the upper 16 bit's
 * can be used by board code.
 *
 * @param[in] clear Bit flags to clear
 * @param[in] set Bit flags to set
 *
 */
void boot_clear_set_flags(uint32_t clear, uint32_t set);

/**
 * Read current boot flags
 *
 * @return Current boot flags
 */
uint32_t boot_get_flags(void);

/**
 * Proxy function to set/activate a boot partition. This function will
 * call the 'set_boot_partition' callback in the config struct if it's
 * populated.
 * 
 * @param[in] part_uu UUID of partition to set as active
 *
 * @return PB_OK on success all other codes are errors
 */
int boot_set_boot_partition(uuid_t part_uu);

/**
 * Proxy function to get active boot partition. This function will
 * call the 'get_boot_partition' callback in the config struct if it's
 * populated.
 * 
 * @param[out] part_uu UUID of active partition
 *
 * @return PB_OK on success all other codes are errors
 */
void boot_get_boot_partition(uuid_t part_uu);

/**
 * Boot the system. This fuction tries to load/auth and execute a boot image
 *
 * @param[in] boot_part_override_uu Optional partition UUID to boot.
 *  Set this to NULL for default behaviour.
 *
 * @return This function will not return on sucess
 */
int boot(uuid_t boot_part_override_uu);


int boot_load(uuid_t boot_part_override_uu);
int boot_jump(void);

#endif
