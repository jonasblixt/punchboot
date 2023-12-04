/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PLAT_QEMU_H
#define INCLUDE_PLAT_QEMU_H

#include <pb/rot.h>
#include <pb/slc.h>

#define FUSE_SRK0   (0)
#define FUSE_SRK1   (1)
#define FUSE_SRK2   (2)
#define FUSE_SRK3   (3)
#define FUSE_SRK4   (4)
#define FUSE_BOOT0  (5)
#define FUSE_BOOT1  (6)

#define FUSE_REVOKE (8)
#define FUSE_SEC    (9)

int board_init(void);

int qemu_revoke_key(const struct rot_key *key);
int qemu_read_key_status(const struct rot_key *key);

slc_t qemu_slc_read_status(void);
int qemu_slc_set_configuration_locked(void);
int qemu_slc_set_eol(void);

#endif // INCLUDE_PLAT_QEMU_H
