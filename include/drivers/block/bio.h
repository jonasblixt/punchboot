/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BIO_H
#define INCLUDE_PB_BIO_H

#include <stdint.h>
#include <uuid.h>

/**
 * Block IO device flags
 *
 * These flags generally apply for the command mode interface, i.e. internally
 * there is always full access to the device. BIO_FLAG_BOOTABLE begin the
 * only exception.
 *
 * \def BIO_FLAG_BOOTABLE
 * This is an acceptable boot device
 *
 * \dev BIO_FLAG_RFU0
 * Reserved for future use
 *
 * \dev BIO_FLAG_WRITEABLE
 * Write access
 *
 * \dev BIO_FLAG_RFU1
 * Reserved for future use
 *
 * \dev BIO_FLAG_RFU2
 * Reserved for future use
 *
 * \dev BIO_FLAG_VISIBLE
 * Block device is visible
 *
 * \dev BIO_FLAG_READABLE
 * Read access
 */

#define BIO_FLAG_BOOTABLE           BIT(0)
#define BIO_FLAG_RFU1               BIT(1)
#define BIO_FLAG_WRITABLE           BIT(2)
#define BIO_FLAG_RFU3               BIT(3)
#define BIO_FLAG_RFU4               BIT(4)
#define BIO_FLAG_VISIBLE            BIT(5)
#define BIO_FLAG_READABLE           BIT(6)
#define BIO_FLAG_RFU7               BIT(7)
#define BIO_FLAG_RFU8               BIT(8)
#define BIO_FLAG_RFU9               BIT(9)
#define BIO_FLAG_RFU10              BIT(10)
#define BIO_FLAG_RFU11              BIT(11)
#define BIO_FLAG_RFU12              BIT(12)
#define BIO_FLAG_RFU13              BIT(13)
#define BIO_FLAG_RFU14              BIT(14)
#define BIO_FLAG_RFU15              BIT(15)

typedef int bio_dev_t;
typedef int (*bio_read_t)(bio_dev_t dev, int lba, size_t length, uintptr_t buf);
typedef int (*bio_write_t)(bio_dev_t dev, int lba, size_t length, const uintptr_t buf);
typedef int (*bio_call_t)(bio_dev_t dev, int param);

/**
 * Allocate a new block device
 *
 * @param[in] first_lba First LBA
 * @param[in] last_lba Last LBA
 * @param[in] block_size Block size in bytes
 * @param[in] uu Device UUID
 * @param[in] description String description of device
 *
 * @return A new block device handle on success
 *         -PB_ERR_MEM, When the block device pool is full
 *         -PB_ERR_PARAM, On invalid lba's or block_size
 */
bio_dev_t bio_allocate(int first_lba, int last_lba, size_t block_size,
                       const uuid_t uu, const char *description);

/**
 * Allocate a new block device from a parent device
 *
 * This will inherit I/O callbacks and flags from the parent device
 *
 * @param[in] parent Parent block device
 * @param[in] first_lba First LBA
 * @param[in] last_lba Last LBA
 * @param[in] block_size Block size in bytes
 * @param[in] uu Device UUID
 * @param[in] description String description of device
 *
 * @return A new block device handle on success
 *         -PB_ERR_IO, On invalid parent device
 *         -PB_ERR_MEM, When the block device pool is full
 *         -PB_ERR_PARAM, On invalid lba's or block_size
 */
bio_dev_t bio_allocate_parent(bio_dev_t parent,
                              int first_lba,
                              int last_lba,
                              size_t block_size,
                              const uuid_t uu,
                              const char *description);

/**
 * Check if a device handle is valid
 *
 * @return true if valid otherwise false
 */
bool bio_valid(bio_dev_t dev);

/**
 * Find block device by uuid
 *
 * @param[in] uu Block device UUID
 *
 * @return Block device handle on success
 *         -PB_ERR_NOT_FOUND, If not device was found
 */
bio_dev_t bio_get_part_by_uu(const uuid_t uu);

/**
 * Find block device by uuid string
 *
 * @param[in] uu_str Block device UUID string
 *
 * @return Block device handle on success
 *         -PB_ERR_NOT_FOUND, If not device was found
 *         -PB_ERR_PARAM, Bad UUID string
 */
bio_dev_t bio_get_part_by_uu_str(const char *uu_str);

/**
 * Set I/O ops for device
 *
 * @param[in] dev Block device handle
 * @param[in] read Read callback function
 * @param[in] write Write callback function
 *
 * @return PB_OK on success,
 *        -PB_ERR_PARAM on invalid device handle
 */
int bio_set_ios(bio_dev_t dev, bio_read_t read, bio_write_t write);

/**
 * Block device size in bytes
 *
 * @param[in] dev Block device handle
 *
 * @return Size in bytes, on success
 *        -PB_ERR_PARAM on invalid device handle
 */
ssize_t bio_size(bio_dev_t dev);

/**
 * Block device, block size in bytes
 *
 * @param[in] dev Block device handle
 *
 * @return Block size in bytes, on success
 *        -PB_ERR_PARAM on invalid device handle
 */
ssize_t bio_block_size(bio_dev_t dev);

/**
 * Get block device description string
 *
 * @param[in] dev Block device handle
 *
 * @return Pointer to description string, on success
 *         NULL, on errors
 */
const char * bio_get_description(bio_dev_t dev);

/**
 * Get HAL flags for device. The HAL flags are indended to be used
 * by the underlying hardware for the block device. Normally the get/set
 * function is only called by the low-level drivers.
 *
 * @param[in] dev Device handle
 *
 * @return Bit flags, on success,
 *         -PB_ERR_PARAM, on bad device handle
 */
int bio_get_hal_flags(bio_dev_t dev);

/**
 * Set HAL flags for device.
 *
 * @param[in] dev Block device handle
 * @param[in] flags Bit flags to set
 *
 * @return PB_OK, on success
 *        -PB_ERR_PARAM, on bad device handle
 */
int bio_set_hal_flags(bio_dev_t dev, uint8_t flags);

/**
 * Get flags from block device
 *
 * @param[in] dev Block device handle
 *
 * @return Flag bits, on success
 *        -PB_ERR_PARAM, on bad device handle
 */
int bio_get_flags(bio_dev_t dev);

/**
 * Set flags on block device
 *
 * @param[in] dev Block device handle
 * @param[in] flags Flags to set
 *
 * @return PB_OK, on success
 *        -PB_ERR_PARAM, on bad device handle
 */
int bio_set_flags(bio_dev_t dev, uint16_t flags);

/**
 * Set and clear flags on block device
 *
 * @param[in] dev Block device handle
 * @param[in] clear_flags Flags to clear
 * @param[in] set_flags Flags to set
 *
 * @return PB_OK, on success
 *        -PB_ERR_PARAM, on bad device handle
 */
int bio_clear_set_flags(bio_dev_t dev, uint16_t clear_flags, uint16_t set_flags);

/**
 * Get UUID of block device
 *
 * @param[in] dev Block device handle
 *
 * @return UUID, on success
 *         NULL, on errors
 */
const unsigned char * bio_get_uu(bio_dev_t dev);

/**
 * Get UUID of block device
 *
 * @param[in] dev Block device handle
 *
 * @return First lba, on success
 *         -PB_ERR_PARAM, on bad device handle
 */
int bio_get_first_block(bio_dev_t dev);

/**
 * Get UUID of block device
 *
 * @param[in] dev Block device handle
 *
 * @return First lba, on success
 *         -PB_ERR_PARAM, on bad device handle
 */
int bio_get_last_block(bio_dev_t dev);

/**
 * Read data from block device
 *
 * @param[in] dev Block device handle
 * @param[in] lba Start block to read from
 * @param[in] length Length in bytes
 * @param[out] buf Output buffer
 *
 * @return -PB_ERR_NOT_SUPPORTED, when there is no underlying read function
 *         -PB_ERR_PARAM, lba and/or length is out of range
 *         -PB_ERR_IO, Driver I/O errors
 *         -PB_TIMEOUT, Driver timeouts
 */
int bio_read(bio_dev_t dev, int lba, size_t length, uintptr_t buf);

/**
 * Write data to block device
 *
 * @param[in] dev Block device handle
 * @param[in] lba Start block to write to
 * @param[in] length Length in bytes
 * @param[in] buf Input buffer
 *
 * @return -PB_ERR_NOT_SUPPORTED, when there is no underlying write function
 *         -PB_ERR_PARAM, lba and/or length is out of range
 *         -PB_ERR_IO, Driver I/O errors
 *         -PB_TIMEOUT, Driverr timeouts
 */
int bio_write(bio_dev_t dev, int lba, size_t length, const uintptr_t buf);

/**
 * Install partition table on a device
 *
 * @param[in] part_uu Destination part uuid
 * @param[in] configuration_index Optional configuration/variant
 *
 * @return PB_OK, on success
 *         -PB_ERR_PARAM, on bad device handle / uuid
 *         -PB_ERR_NOT_SUPPORTED, if there is no callback
 *         -PB_ERR_IO, Driver I/O errors
 *         -PB_TIMEOUT, Driver timeouts
 */
int bio_install_partition_table(uuid_t part_uu, int variant);

/**
 * Sets the partition install callback for 'dev'
 *
 * @param[in] dev Block device handle
 * @param[in] cb Callback function
 *
 * @return PB_OK, on success
 *        -PB_ERR_PARAM, on bad device device handle
 */
int bio_set_install_partition_cb(bio_dev_t dev, bio_call_t cb);


#endif
