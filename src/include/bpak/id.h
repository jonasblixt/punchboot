/**
 * \file utils.h
 *
 * BPAK - Bit Packer
 *
 * Copyright (C) 2022 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef INCLUDE_BPAK_ID_H_
#define INCLUDE_BPAK_ID_H_

#include <stdint.h>
#include <stddef.h>
#include <bpak/bpak.h>

/* Meta data ID's */
#define BPAK_ID_BPAK_TRANSPORT       (0x2d44bbfb)
#define BPAK_ID_MERKLE_SALT          (0x7c9b2f93)
#define BPAK_ID_MERKLE_ROOT_HASH     (0xe68fc9be)
#define BPAK_ID_KEYSTORE_PROVIDER_ID (0xfb367d9a)
#define BPAK_ID_BPAK_VERSION         (0x9a5bab69)
#define BPAK_ID_PB_LOAD_ADDR         (0xd1e64a4b)
#define BPAK_ID_BPAK_PACKAGE         (0xfb2f1f3f)
#define BPAK_ID_BPAK_KEY_ID          (0x7da19399)
#define BPAK_ID_BPAK_KEY_STORE       (0x106c13a7)

/* Algorithm ID's */
#define BPAK_ID_BSDIFF          (0x9f7aacf9)
#define BPAK_ID_BSDIFF_LZMA     (0x1607e56e)
#define BPAK_ID_BSDIFF_NO_COMP  (0x0a878e3e)
#define BPAK_ID_BSPATCH         (0xb5964388)
#define BPAK_ID_BSPATCH_LZMA    (0x933a9893)
#define BPAK_ID_BSPATCH_NO_COMP (0x75622592)
#define BPAK_ID_MERKLE_GENERATE (0xb5bcc58f)
#define BPAK_ID_REMOVE_DATA     (0x57004cd0)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Translate a string to id value
 *
 * @param[in] str Input string
 *
 * @return BPAK ID of \ref str
 */
bpak_id_t bpak_id(const char *str);

/**
 * Return string representaion of known id's
 *
 * @param[in] id BPAK ID
 *
 * @return Textual representation of BPAK ID
 **/
const char *bpak_id_to_string(bpak_id_t id);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // INCLUDE_BPAK_UTILS_H_
