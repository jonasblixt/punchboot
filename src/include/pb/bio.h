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

/* Some well known UUID's */

/* eMMC BOOT0 */
#define UUID_9eef7544_bf68_4bf7_8678_da117cbccba8 (const unsigned char *) "\x9e\xef\x75\x44\xbf\x68\x4b\xf7\x86\x78\xda\x11\x7c\xbc\xcb\xa8"
/* eMMC BOOT1 */
#define UUID_4ee31690_0c9b_4d56_a6a6_e6d6ecfd4d54 (const unsigned char *) "\x4e\xe3\x16\x90\x0c\x9b\x4d\x56\xa6\xa6\xe6\xd6\xec\xfd\x4d\x54"
/* eMMC USER */
#define UUID_1aad85a9_75cd_426d_8dc4_e9bdfeeb6875 (const unsigned char *) "\x1a\xad\x85\xa9\x75\xcd\x42\x6d\x8d\xc4\xe9\xbd\xfe\xeb\x68\x75"
/* eMMC RPMB */
#define UUID_8d75d8b9_b169_4de6_bee0_48abdc95c408 (const unsigned char *) "\x8d\x75\xd8\xb9\xb1\x69\x4d\xe6\xbe\xe0\x48\xab\xdc\x95\xc4\x08"

typedef int bio_dev_t;
typedef int (*bio_read_t)(bio_dev_t dev, int lba, size_t length, uintptr_t buf);
typedef int (*bio_write_t)(bio_dev_t dev, int lba, size_t length, const uintptr_t buf);
typedef int (*bio_call_t)(bio_dev_t dev);

/**
 * Allocate a new block device structure
 *
 * @return A pointer to a new block device struct or NULL on errors
 */
bio_dev_t bio_allocate(int first_lba, int last_lba, size_t block_size,
                       const uuid_t uu, const char *description);

bio_dev_t bio_allocate_parent(bio_dev_t parent,
                              int first_lba,
                              int last_lba,
                              size_t block_size,
                              const uuid_t uu,
                              const char *description);

bio_dev_t bio_part_get_by_uu(const uuid_t uu);
bio_dev_t bio_part_get_by_uu_str(const char *uu_str);

int bio_set_ios(bio_dev_t dev, bio_read_t read, bio_write_t write);
int bio_set_async_ios(bio_dev_t dev, bio_read_t read, bio_write_t write);
ssize_t bio_size(bio_dev_t dev);
ssize_t bio_block_size(bio_dev_t dev);

int bio_get_hal_flags(bio_dev_t dev);
int bio_set_hal_flags(bio_dev_t dev, uint8_t flags);

int bio_get_flags(bio_dev_t dev);
int bio_set_flags(bio_dev_t dev, uint16_t flags);

int bio_read(bio_dev_t dev, int lba, size_t length, uintptr_t buf);
int bio_write(bio_dev_t dev, int lba, size_t length, const uintptr_t buf);

int bio_async_read(bio_dev_t dev, int lba, size_t length, uintptr_t buf);
int bio_async_write(bio_dev_t dev, int lba, size_t length, const uintptr_t buf);
int bio_async_wait(bio_dev_t dev);

#endif
