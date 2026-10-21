/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Force included (-include) in every host build translation unit.
 *
 * The firmware is normally built against its own libc (include/libc), whose
 * <cdefs.h> provides BSD style attribute macros. On the host we build against
 * the system libc instead, which does not define these.
 */

#ifndef PLAT_HOST_COMPAT_H
#define PLAT_HOST_COMPAT_H

#include <sys/types.h>

#define __dead2      __attribute__((__noreturn__))
#define __packed     __attribute__((__packed__))
#define __used       __attribute__((__used__))
#define __unused     __attribute__((__unused__))
#define __aligned(x) __attribute__((__aligned__(x)))
#define __section(x) __attribute__((__section__(x)))
#define __weak       __attribute__((__weak__))

#endif // PLAT_HOST_COMPAT_H
