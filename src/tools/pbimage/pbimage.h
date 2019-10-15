/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef TOOLS_PBIMAGE_PBIMAGE_H_
#define TOOLS_PBIMAGE_PBIMAGE_H_

#include <stdint.h>
#include <stdbool.h>

uint32_t pbimage_prepare(uint32_t key_index,
                         uint32_t hash_kind,
                         uint32_t sign_kind,
                         const char *key_source,
                         const char *pkcs11_provider,
                         const char *pkcs11_key_id,
                         const char *output_fn);

void pbimage_cleanup(void);

uint32_t pbimage_append_component(const char *comp_type,
                                  uint32_t load_addr,
                                  const char *fn);

uint32_t pbimage_out(const char *fn);

#endif  // TOOLS_PBIMAGE_PBIMAGE_H_
