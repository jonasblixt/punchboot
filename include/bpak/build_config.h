/**
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The build configuration of the vendored bpak library, which upstream
 * generates from lib/build_config.h.in. punchboot only vendors the header
 * functions (src/lib/bpak.c), without merkle, lzma or the mbedtls backend.
 * BPAK_SIZEOF_OFF_T is not defined: the library is compiled into the
 * program that uses it, so the size of off_t can not differ.
 */

#ifndef BPAK_BUILD_CONFIG_H
#define BPAK_BUILD_CONFIG_H

#define BPAK_CONFIG_MERKLE  0
#define BPAK_CONFIG_LZMA    0
#define BPAK_CONFIG_MBEDTLS 0

#endif
