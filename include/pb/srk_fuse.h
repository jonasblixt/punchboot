/**
 * \file srk_fuse.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The SRK hash the ROM checks pb's signature against, as the words to fuse.
 * Generated at build time from CONFIG_SRK_FUSE (srktool's *_fuse.bin), so it
 * matches the keys pb is signed with. Without CONFIG_SRK_FUSE it is empty
 * (srk_fuse_words 0). The platform's set_configuration fuses it.
 */

#ifndef INCLUDE_PB_SRK_FUSE_H
#define INCLUDE_PB_SRK_FUSE_H

#include <stddef.h>
#include <stdint.h>

extern const uint32_t srk_fuse[];
extern const size_t srk_fuse_words;

#endif // INCLUDE_PB_SRK_FUSE_H
