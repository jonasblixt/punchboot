/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_QEMU_FUSE_H_
#define PLAT_QEMU_FUSE_H_

#include <pb/pb.h>

int qemu_fuse_init(void);

int qemu_fuse_write(uint32_t id, uint32_t val);

int qemu_fuse_read(uint32_t id, uint32_t *val);

#endif  // PLAT_TEST_TEST_FUSE_H_
