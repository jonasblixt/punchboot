/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <plat/imx6ul/rom_log.h>
#include <stdio.h>
#include <string.h>

#define ROM_LOG_PTR       0x000001e0
#define ROM_LOG_MAX_WORDS 64
#define OCRAM_START       0x00900000
#define OCRAM_END         0x00920000

/*
 * Event IDs and parameters from NXP AN12853 "i.MX ROMs Log Events", Table 7
 * (ROM event ID definition version 0, the i.MX6 series). An entry is an event
 * word, the ID in bits 23:0, followed by its parameter words. Unknown events
 * are printed as raw words.
 */
#define ROM_LOG_EVENT_ID  GENMASK_32(23, 0)

enum rom_log_arg {
    ARG_NONE,
    ARG_HEX,
    ARG_AUTH,
};

struct rom_log_event {
    uint32_t id;
    enum rom_log_arg arg;
    const char *name;
};

static const struct rom_log_event rom_log_events[] = {
    { 0x00010000, ARG_NONE, "Boot mode: boot from fuses" },
    { 0x00010001, ARG_NONE, "Boot mode: serial download" },
    { 0x00010002, ARG_NONE, "Boot mode: internal boot" },
    { 0x00010003, ARG_NONE, "Boot mode: test mode" },
    { 0x00020000, ARG_NONE, "Security config: FAB" },
    { 0x00020033, ARG_NONE, "Security config: field return" },
    { 0x000200f0, ARG_NONE, "Security config: open" },
    { 0x000200cc, ARG_NONE, "Security config: closed" },
    { 0x00030000, ARG_NONE, "Internal use" },
    { 0x00030001, ARG_NONE, "Internal use" },
    { 0x00040000, ARG_NONE, "FUSE_SEL_VALUE fuse not blown" },
    { 0x00040001, ARG_NONE, "FUSE_SEL_VALUE fuse blown" },
    { IMX6UL_ROM_LOG_PRIMARY_IMAGE, ARG_NONE, "Boot from the primary image" },
    { 0x00050001, ARG_NONE, "Boot from the secondary image" },
    { 0x00060000, ARG_NONE, "Primary boot device: raw NAND" },
    { IMX6UL_ROM_LOG_PRIMARY_SD_MMC, ARG_NONE, "Primary boot device: SD/eMMC" },
    { 0x00060003, ARG_NONE, "Primary boot device: I2C EEPROM" },
    { 0x00060004, ARG_NONE, "Primary boot device: ECSPI NOR" },
    { 0x00060005, ARG_NONE, "Primary boot device: WEIM NOR" },
    { 0x00060006, ARG_NONE, "Primary boot device: OneNAND" },
    { 0x00060007, ARG_NONE, "Primary boot device: QSPI NOR" },
    { 0x00061003, ARG_NONE, "Recovery boot device: I2C EEPROM" },
    { 0x00061004, ARG_NONE, "Recovery boot device: ECSPI NOR" },
    { 0x00061fff, ARG_NONE, "No recovery boot device" },
    { 0x00062001, ARG_NONE, "Manufacture boot device: SD/eMMC" },
    { 0x00070000, ARG_NONE, "Device init" },
    { 0x000700f0, ARG_NONE, "Device init: pass" },
    { IMX6UL_ROM_LOG_DEVICE_INIT_FAIL, ARG_NONE, "Device init: fail" },
    { 0x00080000, ARG_HEX, "Device read, image offset" },
    { 0x000800f0, ARG_NONE, "Device read: pass" },
    { IMX6UL_ROM_LOG_DEVICE_READ_FAIL, ARG_NONE, "Device read: fail" },
    { 0x00090000, ARG_AUTH, "Authentication" },
    { 0x000a0000, ARG_HEX, "Run plugin" },
    { 0x000a00f0, ARG_NONE, "Plugin: pass" },
    { 0x000a0033, ARG_NONE, "Plugin: fail" },
    { IMX6UL_ROM_LOG_JUMP, ARG_HEX, "Jump to boot image" },
    { IMX6UL_ROM_LOG_SERIAL_DOWNLOAD, ARG_NONE, "Serial download" },
    { IMX6UL_ROM_LOG_JUMP_SDP, ARG_HEX, "Jump to serial download image" },
    { 0x000e0000, ARG_NONE, "Internal use" },
};

static uint32_t rom_log[ROM_LOG_MAX_WORDS];
static size_t rom_log_words;
static uintptr_t rom_log_addr;

static const struct rom_log_event *find_event(uint32_t word)
{
    uint32_t id = word & ROM_LOG_EVENT_ID;

    for (size_t i = 0; i < ARRAY_SIZE(rom_log_events); i++) {
        if (rom_log_events[i].id == id)
            return &rom_log_events[i];
    }

    return NULL;
}

/*
 * The firmware snprintf only knows %d, %i, %s, %u and %x, and %x doesn't
 * check the buffer size. A line is only written with ROM_LOG_LINE_MAX bytes left.
 */
#define ROM_LOG_LINE_MAX 128

int imx6ul_rom_log_decode(const uint32_t *log,
                          size_t n_words,
                          uintptr_t log_addr,
                          char *out,
                          size_t *out_len)
{
    size_t out_size = *out_len;
    size_t pos = 0;

    *out_len = 0;
    if (out_size < ROM_LOG_LINE_MAX)
        return -PB_ERR_BUF_TOO_SMALL;

    pos += snprintf(out, out_size, "ROM log at 0x%08x\n", (unsigned int)log_addr);

    for (size_t i = 0; i < n_words && log[i] != 0; i++) {
        const struct rom_log_event *ev = find_event(log[i]);
        unsigned int idx = (unsigned int)i;
        /* No field widths in the firmware snprintf */
        const char *pad = (idx < 10) ? " " : "";
        char *line = out + pos;

        if (out_size - pos < ROM_LOG_LINE_MAX) {
            *out_len = pos;
            return -PB_ERR_BUF_TOO_SMALL;
        }

        if (ev == NULL) {
            pos += snprintf(
                line, ROM_LOG_LINE_MAX, "%s%u: %08x           Unknown event\n", pad, idx, log[i]);
        } else if (ev->arg == ARG_NONE) {
            pos += snprintf(
                line, ROM_LOG_LINE_MAX, "%s%u: %08x           %s\n", pad, idx, log[i], ev->name);
        } else if (i + 1 >= n_words) {
            pos += snprintf(line,
                            ROM_LOG_LINE_MAX,
                            "%s%u: %08x           %s: parameter missing\n",
                            pad,
                            idx,
                            log[i],
                            ev->name);
        } else if (ev->arg == ARG_AUTH) {
            uint32_t arg = log[++i];

            /* Bits 7:0 are 0xf0 if the image passed */
            pos += snprintf(line,
                            ROM_LOG_LINE_MAX,
                            "%s%u: %08x %08x  %s: %s\n",
                            pad,
                            idx,
                            log[idx],
                            arg,
                            ev->name,
                            ((arg & 0xff) == 0xf0) ? "pass" : "fail");
        } else {
            uint32_t arg = log[++i];

            pos += snprintf(line,
                            ROM_LOG_LINE_MAX,
                            "%s%u: %08x %08x  %s: 0x%08x\n",
                            pad,
                            idx,
                            log[idx],
                            arg,
                            ev->name,
                            arg);
        }
    }

    *out_len = pos;
    return PB_OK;
}

void imx6ul_rom_log_save(void)
{
    uintptr_t ptr_addr = ROM_LOG_PTR;
    uintptr_t log;

    /* A low constant address looks like NULL to GCC, see hab_rvt_read() */
    __asm__("" : "+r"(ptr_addr));
    log = *(const volatile uint32_t *)ptr_addr;
    rom_log_addr = log;

    if (log < OCRAM_START || log > OCRAM_END - sizeof(rom_log) || (log & 3)) {
        rom_log_words = 0;
        return;
    }

    for (size_t i = 0; i < ROM_LOG_MAX_WORDS; i++)
        rom_log[i] = ((const volatile uint32_t *)log)[i];

    rom_log_words = ROM_LOG_MAX_WORDS;
}

bool imx6ul_rom_log_find(const uint32_t *log, size_t n_words, uint32_t event)
{
    for (size_t i = 0; i < n_words && log[i] != 0; i++) {
        const struct rom_log_event *ev = find_event(log[i]);

        if ((log[i] & ROM_LOG_EVENT_ID) == event)
            return true;
        if (ev != NULL && ev->arg != ARG_NONE)
            i++; /* Skip the parameter */
    }

    return false;
}

bool imx6ul_rom_log_has_event(uint32_t event)
{
    return imx6ul_rom_log_find(rom_log, rom_log_words, event);
}

int imx6ul_rom_log_dump(char *out, size_t *out_len)
{
    if (rom_log_words == 0) {
        LOG_ERR("No ROM log (pointer 0x%08x)", (unsigned int)rom_log_addr);
        *out_len = 0;
        return -PB_ERR_NOT_FOUND;
    }

    return imx6ul_rom_log_decode(rom_log, rom_log_words, rom_log_addr, out, out_len);
}
