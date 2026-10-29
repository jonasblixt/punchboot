/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX boot ROM's event log, from NXP AN12853 "i.MX ROMs Log Events".
 * The ROM records what it did (boot mode, boot device, image loads,
 * authentication, ...) as event words in on-chip RAM. A platform saves the
 * log early, before anything overwrites it, and pb decodes it as text.
 */

#ifndef INCLUDE_DRIVERS_ROM_LOG_IMX_ROM_LOG_H
#define INCLUDE_DRIVERS_ROM_LOG_IMX_ROM_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The most words a log can have here (AN12853 Table 13) */
#define IMX_ROM_LOG_MAX_WORDS           128

/* ROM event ID definition versions (AN12853 Table 6) */
#define IMX_ROM_LOG_DEFS_V0             0 /* i.MX6 series, IDs in bits 23:0 */

/* Events used outside the decoder, definition version 0 (AN12853 Table 7) */
#define IMX_ROM_LOG_V0_PRIMARY_IMAGE    0x00050000
#define IMX_ROM_LOG_V0_PRIMARY_SD_MMC   0x00060001
#define IMX_ROM_LOG_V0_DEVICE_INIT_FAIL 0x00070033
#define IMX_ROM_LOG_V0_DEVICE_READ_FAIL 0x00080033
#define IMX_ROM_LOG_V0_JUMP             0x000b0000
#define IMX_ROM_LOG_V0_SERIAL_DOWNLOAD  0x000c0000
#define IMX_ROM_LOG_V0_JUMP_SDP         0x000d0000

/** Where a SoC's ROM keeps its log */
struct imx_rom_log_config {
    uintptr_t pointer; /*!< Address of the pointer to the log (AN12853 Table 11) */
    uintptr_t ram_start; /*!< The log must be in [ram_start, ram_end) */
    uintptr_t ram_end;
    size_t words; /*!< Size of the log (AN12853 Table 13), at most IMX_ROM_LOG_MAX_WORDS */
    unsigned int defs; /*!< ROM event ID definition version, IMX_ROM_LOG_DEFS_* */
};

/**
 * Copy the ROM's log, before anything can overwrite it. When there is a log,
 * it can be read as the log "rom" (PB_CMD_LOG_READ).
 *
 * @param[in] cfg Where the log is, must stay valid
 */
void imx_rom_log_save(const struct imx_rom_log_config *cfg);

/**
 * Decode the log saved by imx_rom_log_save() as text
 *
 * @param[out] out Output buffer
 * @param[in,out] out_len Size of out, and the length of the text
 *
 * @return PB_OK on success,
 *         -PB_ERR_NOT_FOUND if nothing was saved or the log pointer was bad,
 *         -PB_ERR_BUF_TOO_SMALL if the text was truncated
 */
int imx_rom_log_dump(char *out, size_t *out_len);

/**
 * Check if the log saved by imx_rom_log_save() has an event
 *
 * @param[in] event Event ID, as in the AN12853 tables
 *
 * @return true if the event was logged, false if not or there is no log
 */
bool imx_rom_log_has_event(uint32_t event);

/**
 * Decode ROM log words as text, one event per line
 *
 * Decoding stops at the first zero event or after n_words.
 *
 * @param[in] defs ROM event ID definition version, IMX_ROM_LOG_DEFS_*
 * @param[in] log Log words
 * @param[in] n_words Number of words in log
 * @param[in] log_addr Address the log was read from, for the header
 * @param[out] out Output buffer
 * @param[in,out] out_len Size of out, and the length of the text
 *
 * @return PB_OK on success, -PB_ERR_BUF_TOO_SMALL if the text was truncated
 */
int imx_rom_log_decode(unsigned int defs,
                       const uint32_t *log,
                       size_t n_words,
                       uintptr_t log_addr,
                       char *out,
                       size_t *out_len);

/**
 * Check if ROM log words have an event. Parameter words are skipped, so they
 * never match.
 *
 * @param[in] defs ROM event ID definition version, IMX_ROM_LOG_DEFS_*
 * @param[in] log Log words
 * @param[in] n_words Number of words in log
 * @param[in] event Event ID, as in the AN12853 tables
 *
 * @return true if the event is in the log
 */
bool imx_rom_log_find(unsigned int defs, const uint32_t *log, size_t n_words, uint32_t event);

#endif // INCLUDE_DRIVERS_ROM_LOG_IMX_ROM_LOG_H
