/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_SOCKET_PROTO_H_
#define PLAT_TEST_SOCKET_PROTO_H_

#include <stdint.h>

struct pb_socket_header
{
    uint32_t ep;
    uint32_t sz;
} __attribute__((packed));

#endif  // PLAT_TEST_SOCKET_PROTO_H_
