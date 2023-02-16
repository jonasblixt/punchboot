/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BOOT_H_
#define INCLUDE_PB_BOOT_H_

#include <stdint.h>
#include <pb/image.h>
#include <pb/storage.h>
#include <pb/time.h>
#include <bpak/bpak.h>

#define PB_STATE_MAGIC 0x026d4a65

struct pb_boot_state /* 512 bytes */
{
    uint32_t magic;
    uint8_t private[468];
    uint8_t rz[36];
    uint32_t crc;
} __attribute__((packed));

int pb_boot_init(void);

int pb_boot_load_state(void);

int pb_boot_load_transport(void);

int pb_boot_load_fs(uint8_t *boot_part_uu);

int pb_boot(struct pb_timestamp *ts_total,
            bool verbose,
            bool manual);

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

#endif  // INCLUDE_PB_BOOT_H_
