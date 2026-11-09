/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <drivers/rom_log/imx_rom_log.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <stdio.h>
#include <string.h>

/*
 * Event IDs and parameters from NXP AN12853 "i.MX ROMs Log Events". An entry
 * is an event word followed by its parameter words. Where the ID is in the
 * word depends on the ROM event ID format (Table 3), the IDs on the
 * definition version (Table 6). Unknown events are printed as raw words.
 */

/* What follows an event word */
enum rom_log_param {
    P_NONE,
    P_HEX, /* A word, printed in hex */
    P_AUTH, /* A word, bits 7:0 are 0xf0 if the image passed */
    P_TICK, /* A word, the time (the unit isn't documented) */
    P_VERSION, /* No word: bits 7:0 of the event word are a version */
};

#define ROM_LOG_MAX_PARAMS 2

struct rom_log_event {
    uint32_t id;
    const char *name;
    enum rom_log_param params[ROM_LOG_MAX_PARAMS];
};

/* Events with 0, 1 and 2 parameters */
#define EV0(id, name)      \
    {                      \
        (id), (name),      \
        {                  \
            P_NONE, P_NONE \
        }                  \
    }
#define EV1(id, name, p) \
    {                    \
        (id), (name),    \
        {                \
            (p), P_NONE  \
        }                \
    }
#define EV2(id, name, p0, p1) \
    {                         \
        (id), (name),         \
        {                     \
            (p0), (p1)        \
        }                     \
    }

/* Definition version 0, the i.MX6 series (Table 7) */
static const struct rom_log_event events_v0[] = {
    EV0(0x00010000, "Boot mode: boot from fuses"),
    EV0(0x00010001, "Boot mode: serial download"),
    EV0(0x00010002, "Boot mode: internal boot"),
    EV0(0x00010003, "Boot mode: test mode"),
    EV0(0x00020000, "Security config: FAB"),
    EV0(0x00020033, "Security config: field return"),
    EV0(0x000200f0, "Security config: open"),
    EV0(0x000200cc, "Security config: closed"),
    EV0(0x00030000, "Internal use"),
    EV0(0x00030001, "Internal use"),
    EV0(0x00040000, "FUSE_SEL_VALUE fuse not blown"),
    EV0(0x00040001, "FUSE_SEL_VALUE fuse blown"),
    EV0(IMX_ROM_LOG_V0_PRIMARY_IMAGE, "Boot from the primary image"),
    EV0(0x00050001, "Boot from the secondary image"),
    EV0(0x00060000, "Primary boot device: raw NAND"),
    EV0(IMX_ROM_LOG_V0_PRIMARY_SD_MMC, "Primary boot device: SD/eMMC"),
    EV0(0x00060003, "Primary boot device: I2C EEPROM"),
    EV0(0x00060004, "Primary boot device: ECSPI NOR"),
    EV0(0x00060005, "Primary boot device: WEIM NOR"),
    EV0(0x00060006, "Primary boot device: OneNAND"),
    EV0(0x00060007, "Primary boot device: QSPI NOR"),
    EV0(0x00061003, "Recovery boot device: I2C EEPROM"),
    EV0(0x00061004, "Recovery boot device: ECSPI NOR"),
    EV0(0x00061fff, "No recovery boot device"),
    EV0(0x00062001, "Manufacture boot device: SD/eMMC"),
    EV0(0x00070000, "Device init"),
    EV0(0x000700f0, "Device init: pass"),
    EV0(IMX_ROM_LOG_V0_DEVICE_INIT_FAIL, "Device init: fail"),
    EV1(0x00080000, "Device read, image offset", P_HEX),
    EV0(0x000800f0, "Device read: pass"),
    EV0(IMX_ROM_LOG_V0_DEVICE_READ_FAIL, "Device read: fail"),
    EV1(0x00090000, "Authentication", P_AUTH),
    EV1(0x000a0000, "Run plugin", P_HEX),
    EV0(0x000a00f0, "Plugin: pass"),
    EV0(0x000a0033, "Plugin: fail"),
    EV1(IMX_ROM_LOG_V0_JUMP, "Jump to boot image", P_HEX),
    EV0(IMX_ROM_LOG_V0_SERIAL_DOWNLOAD, "Serial download"),
    EV1(IMX_ROM_LOG_V0_JUMP_SDP, "Jump to serial download image", P_HEX),
    EV0(0x000e0000, "Internal use"),
};

/* Definition version 2, the i.MX 8M ("mSCALE") series (Table 9) */
static const struct rom_log_event events_v2[] = {
    EV1(0x01, "ROM event version", P_VERSION),
    EV0(0x10, "Boot mode: boot from fuses"),
    EV0(0x11, "Boot mode: serial download"),
    EV0(0x12, "Boot mode: internal boot"),
    EV0(0x13, "Boot mode: test mode"),
    EV0(0x20, "Security config: FAB"),
    EV0(0x21, "Security config: field return"),
    EV0(0x22, "Security config: open"),
    EV0(0x23, "Security config: closed"),
    EV0(0x30, "Internal use"),
    EV0(0x31, "Internal use"),
    EV0(0x40, "FUSE_SEL_VALUE fuse not blown"),
    EV0(0x41, "FUSE_SEL_VALUE fuse blown"),
    EV0(0x50, "Boot from the primary image"),
    EV0(0x51, "Boot from the secondary image"),
    EV0(0x60, "Primary boot device: raw NAND"),
    EV0(0x61, "Primary boot device: SD/eMMC"),
    EV0(0x64, "Primary boot device: ECSPI NOR"),
    EV0(0x67, "Primary boot device: QSPI NOR"),
    EV0(0x71, "Recovery boot device: ECSPI NOR"),
    EV0(0x72, "No recovery boot device"),
    EV0(0x73, "Manufacture boot device: SD/eMMC"),
    EV0(0x74, "Primary boot device: SPI NAND"),
    EV1(0x80, "Device init", P_TICK),
    EV1(0x81, "Device init: pass", P_TICK),
    /* Table 9 has no parameter, but the 8MP EVK's ROM logs a tick */
    EV1(0x82, "Device pre-config", P_TICK),
    EV0(0x83, "Device pre-config: pass"),
    EV0(0x8e, "Device pre-config: fail"),
    EV1(0x8f, "Device init: fail", P_TICK),
    EV1(0x90, "Device read, image offset", P_HEX),
    EV1(0x91, "Device read: pass", P_TICK),
    EV1(0x9f, "Device read: fail", P_TICK),
    EV2(0xa0, "Authentication", P_AUTH, P_TICK),
    EV0(0xa1, "IVT header not valid"),
    EV2(0xc0, "Jump to boot image", P_HEX, P_TICK),
    EV0(0xd0, "Serial download"),
    EV0(0xe0, "Internal use"),
    EV0(0xf0, "ROM exception handler"),
};

struct rom_log_defs {
    unsigned int version;
    uint32_t id_mask; /* Where the ID is in an event word */
    unsigned int id_shift;
    const struct rom_log_event *events;
    size_t n_events;
};

static const struct rom_log_defs rom_log_defs[] = {
    {
        .version = IMX_ROM_LOG_DEFS_V0,
        .id_mask = GENMASK_32(23, 0), /* Format version 0 */
        .id_shift = 0,
        .events = events_v0,
        .n_events = ARRAY_SIZE(events_v0),
    },
    {
        .version = IMX_ROM_LOG_DEFS_V2,
        .id_mask = GENMASK_32(31, 24), /* Format version 1 */
        .id_shift = 24,
        .events = events_v2,
        .n_events = ARRAY_SIZE(events_v2),
    },
};

/* Lines are at most about 64 characters, most events have no parameter */
#define ROM_LOG_TEXT_SIZE (IMX_ROM_LOG_MAX_WORDS * 64)

static const struct imx_rom_log_config *rom_log_cfg;
static uint32_t rom_log[IMX_ROM_LOG_MAX_WORDS];
static size_t rom_log_words;
static uintptr_t rom_log_addr;
static char rom_log_text[ROM_LOG_TEXT_SIZE];
static size_t rom_log_text_len;
static bool rom_log_decoded;

static const struct rom_log_defs *find_defs(unsigned int version)
{
    for (size_t i = 0; i < ARRAY_SIZE(rom_log_defs); i++) {
        if (rom_log_defs[i].version == version)
            return &rom_log_defs[i];
    }

    return NULL;
}

static uint32_t event_id(const struct rom_log_defs *defs, uint32_t word)
{
    return (word & defs->id_mask) >> defs->id_shift;
}

static const struct rom_log_event *find_event(const struct rom_log_defs *defs, uint32_t word)
{
    uint32_t id = event_id(defs, word);

    for (size_t i = 0; i < defs->n_events; i++) {
        if (defs->events[i].id == id)
            return &defs->events[i];
    }

    return NULL;
}

/*
 * The firmware snprintf only knows %d, %i, %s, %u and %x, and %x doesn't
 * check the buffer size. A line is only written with ROM_LOG_LINE_MAX bytes left.
 */
#define ROM_LOG_LINE_MAX 128

/* Parameter words that follow an event */
static size_t param_words(const struct rom_log_event *ev)
{
    size_t n = 0;

    for (size_t i = 0; i < ROM_LOG_MAX_PARAMS; i++) {
        if (ev->params[i] != P_NONE && ev->params[i] != P_VERSION)
            n++;
    }

    return n;
}

/* Append ': value' for a parameter, ', value' after the first */
static size_t print_param(char *out, enum rom_log_param p, uint32_t value, bool first)
{
    const char *sep = first ? ": " : ", ";

    switch (p) {
    case P_HEX:
        return snprintf(out, ROM_LOG_LINE_MAX / 2, "%s0x%08x", sep, value);
    case P_AUTH:
        return snprintf(
            out, ROM_LOG_LINE_MAX / 2, "%s%s", sep, ((value & 0xff) == 0xf0) ? "pass" : "fail");
    case P_TICK:
        return snprintf(out, ROM_LOG_LINE_MAX / 2, "%stick %u", sep, value);
    case P_VERSION:
        return snprintf(out, ROM_LOG_LINE_MAX / 2, "%s%u", sep, value & 0xff);
    default:
        return 0;
    }
}

/* One event and its parameters as a line, returns the words it took */
static size_t print_event(char *line,
                          size_t *len,
                          const struct rom_log_event *ev,
                          const uint32_t *log,
                          size_t idx,
                          size_t n_words)
{
    /* No field widths in the firmware snprintf */
    const char *pad = (idx < 10) ? " " : "";
    size_t n_params = (ev != NULL) ? param_words(ev) : 0;
    size_t pos;

    if (ev == NULL) {
        *len = snprintf(line,
                        ROM_LOG_LINE_MAX,
                        "%s%u: %08x           Unknown event\n",
                        pad,
                        (unsigned int)idx,
                        log[idx]);
        return 1;
    }

    if (idx + n_params >= n_words) {
        *len = snprintf(line,
                        ROM_LOG_LINE_MAX,
                        "%s%u: %08x           %s: parameter missing\n",
                        pad,
                        (unsigned int)idx,
                        log[idx],
                        ev->name);
        return n_words - idx;
    }

    pos = snprintf(line, ROM_LOG_LINE_MAX, "%s%u: %08x", pad, (unsigned int)idx, log[idx]);
    for (size_t i = 0; i < n_params; i++)
        pos += snprintf(&line[pos], ROM_LOG_LINE_MAX - pos, " %08x", log[idx + 1 + i]);
    /* Line up the names of events with up to one parameter */
    pos += snprintf(
        &line[pos], ROM_LOG_LINE_MAX - pos, (n_params == 0) ? "           %s" : "  %s", ev->name);

    for (size_t i = 0, word = 1; i < ROM_LOG_MAX_PARAMS && ev->params[i] != P_NONE; i++) {
        uint32_t value = (ev->params[i] == P_VERSION) ? log[idx] : log[idx + word++];

        /* "name: value", or "name: state, value" when the name has a state */
        pos += print_param(&line[pos], ev->params[i], value, i == 0 && !strchr(ev->name, ':'));
    }

    pos += snprintf(&line[pos], ROM_LOG_LINE_MAX - pos, "\n");
    *len = pos;
    return 1 + n_params;
}

int imx_rom_log_decode(unsigned int version,
                       const uint32_t *log,
                       size_t n_words,
                       uintptr_t log_addr,
                       char *out,
                       size_t *out_len)
{
    const struct rom_log_defs *defs = find_defs(version);
    size_t out_size = *out_len;
    size_t pos = 0;

    *out_len = 0;

    if (defs == NULL)
        return -PB_ERR_PARAM;
    if (out_size < ROM_LOG_LINE_MAX)
        return -PB_ERR_BUF_TOO_SMALL;

    pos += snprintf(out, out_size, "ROM log at 0x%08x\n", (unsigned int)log_addr);

    for (size_t i = 0; i < n_words && log[i] != 0;) {
        size_t len;

        if (out_size - pos < ROM_LOG_LINE_MAX) {
            *out_len = pos;
            return -PB_ERR_BUF_TOO_SMALL;
        }

        i += print_event(&out[pos], &len, find_event(defs, log[i]), log, i, n_words);
        pos += len;
    }

    *out_len = pos;
    return PB_OK;
}

/* The text of the saved log, decoded at the first read */
static size_t rom_log_text_length(void)
{
    if (!rom_log_decoded) {
        rom_log_text_len = sizeof(rom_log_text);
        /* A truncated log still has what fits */
        (void)imx_rom_log_dump(rom_log_text, &rom_log_text_len);
        rom_log_decoded = true;
    }

    return rom_log_text_len;
}

static uint32_t rom_log_head(void)
{
    return (uint32_t)rom_log_text_length();
}

static size_t rom_log_read(size_t offset, void *buf, size_t length)
{
    size_t size = rom_log_text_length();

    if (offset >= size)
        return 0;

    length = MIN(length, size - offset);
    memcpy(buf, &rom_log_text[offset], length);
    return length;
}

static const struct log_source rom_log_source = {
    .name = "rom",
    .description = "The boot ROM's event log",
    .head = rom_log_head,
    .length = rom_log_text_length,
    .read = rom_log_read,
};

void imx_rom_log_save(const struct imx_rom_log_config *cfg)
{
    uintptr_t ptr_addr = cfg->pointer;
    size_t words = MIN(cfg->words, (size_t)IMX_ROM_LOG_MAX_WORDS);
    uintptr_t log;

    rom_log_cfg = cfg;

    /* A low constant address looks like NULL to GCC, see hab_rvt_read() */
    __asm__("" : "+r"(ptr_addr));
    log = *(const volatile uint32_t *)ptr_addr;
    rom_log_addr = log;

    if (log < cfg->ram_start || log > cfg->ram_end - words * sizeof(uint32_t) || (log & 3)) {
        rom_log_words = 0;
        return;
    }

    for (size_t i = 0; i < words; i++)
        rom_log[i] = ((const volatile uint32_t *)log)[i];

    rom_log_words = words;
    rom_log_decoded = false;

    if (log_source_register(&rom_log_source) != PB_OK)
        LOG_ERR("No room for the ROM log in the logs");
}

bool imx_rom_log_find(unsigned int version, const uint32_t *log, size_t n_words, uint32_t event)
{
    const struct rom_log_defs *defs = find_defs(version);

    if (defs == NULL)
        return false;

    for (size_t i = 0; i < n_words && log[i] != 0; i++) {
        const struct rom_log_event *ev = find_event(defs, log[i]);

        if (event_id(defs, log[i]) == event)
            return true;
        if (ev != NULL)
            i += param_words(ev); /* Skip the parameters */
    }

    return false;
}

bool imx_rom_log_has_event(uint32_t event)
{
    if (rom_log_words == 0)
        return false;

    return imx_rom_log_find(rom_log_cfg->defs, rom_log, rom_log_words, event);
}

int imx_rom_log_dump(char *out, size_t *out_len)
{
    if (rom_log_words == 0) {
        LOG_ERR("No ROM log (pointer 0x%08x)", (unsigned int)rom_log_addr);
        *out_len = 0;
        return -PB_ERR_NOT_FOUND;
    }

    return imx_rom_log_decode(
        rom_log_cfg->defs, rom_log, rom_log_words, rom_log_addr, out, out_len);
}
