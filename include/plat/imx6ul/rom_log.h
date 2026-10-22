/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PLAT_IMX6UL_ROM_LOG_H
#define INCLUDE_PLAT_IMX6UL_ROM_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ROM log event IDs used outside the decoder, from NXP AN12853 Table 7 */
#define IMX6UL_ROM_LOG_PRIMARY_IMAGE    0x00050000
#define IMX6UL_ROM_LOG_PRIMARY_SD_MMC   0x00060001
#define IMX6UL_ROM_LOG_DEVICE_INIT_FAIL 0x00070033
#define IMX6UL_ROM_LOG_DEVICE_READ_FAIL 0x00080033
#define IMX6UL_ROM_LOG_JUMP             0x000b0000
#define IMX6UL_ROM_LOG_SERIAL_DOWNLOAD  0x000c0000
#define IMX6UL_ROM_LOG_JUMP_SDP         0x000d0000

/**
 * Copy the boot ROM's event log, before anything can overwrite it.
 *
 * The ROM keeps a pointer to its log at 0x1E0 (the ROM memory map in the
 * reference manual, and AN12853), the log itself is 64 words in OCRAM.
 */
void imx6ul_rom_log_save(void);

/**
 * Decode the log saved by imx6ul_rom_log_save as text
 *
 * @param[out] out Output buffer
 * @param[in,out] out_len Size of out, and the length of the text
 *
 * @return PB_OK on success,
 *         -PB_ERR_NOT_FOUND if there was no valid log pointer,
 *         -PB_ERR_BUF_TOO_SMALL if the text was truncated
 */
int imx6ul_rom_log_dump(char *out, size_t *out_len);

/**
 * Decode ROM log words as text, one event per line
 *
 * Decoding stops at the first zero event or after n_words.
 *
 * @param[in] log Log words
 * @param[in] n_words Number of words in log
 * @param[in] log_addr Address the log was read from, for the header
 * @param[out] out Output buffer
 * @param[in,out] out_len Size of out, and the length of the text
 *
 * @return PB_OK on success, -PB_ERR_BUF_TOO_SMALL if the text was truncated
 */
int imx6ul_rom_log_decode(const uint32_t *log,
                          size_t n_words,
                          uintptr_t log_addr,
                          char *out,
                          size_t *out_len);

/**
 * Check if the log saved by imx6ul_rom_log_save has an event
 *
 * @param[in] event Event ID
 *
 * @return true if the event was logged, false if not or there is no log
 */
bool imx6ul_rom_log_has_event(uint32_t event);

/**
 * Check if ROM log words have an event. Parameter words are skipped, so they
 * never match.
 *
 * @param[in] log Log words
 * @param[in] n_words Number of words in log
 * @param[in] event Event ID, bits 23:0
 *
 * @return true if the event is in the log
 */
bool imx6ul_rom_log_find(const uint32_t *log, size_t n_words, uint32_t event);

#endif // INCLUDE_PLAT_IMX6UL_ROM_LOG_H
