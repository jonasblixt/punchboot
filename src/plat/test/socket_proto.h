
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __PB_SOCKET_PROTO_H__
#define __PB_SOCKET_PROTO_H__

#include <stdint.h>

struct pb_socket_header
{
    uint32_t ep;
    uint32_t sz;
} __attribute__ ((packed));

#endif
