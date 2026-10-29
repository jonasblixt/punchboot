/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Linux's ramoops (fs/pstore/ram.c, checked against 6.18) splits its region
 * into the dmesg records, then the console, ftrace and pmsg zones. Each zone
 * is a struct persistent_ram_buffer (fs/pstore/ram_core.c): a signature, the
 * position of the next write and the bytes used, then a ring buffer of text.
 * Only the console zone is read here; the dmesg records may be compressed.
 */

#include <boot/linux_ramoops.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <string.h>

#define PERSISTENT_RAM_SIG 0x43474244 /* "DBGC" */

struct persistent_ram_buffer {
    uint32_t sig;
    uint32_t start; /* Where the next byte goes */
    uint32_t size; /* Bytes in the buffer, up to its size */
    uint8_t data[];
};

static const struct persistent_ram_buffer *console;
static size_t console_capacity;

/* Bytes of console text, 0 if the zone has no valid header */
static size_t console_length(void)
{
    if (console == NULL || console->sig != PERSISTENT_RAM_SIG)
        return 0;
    if (console->size > console_capacity || console->start >= console_capacity)
        return 0;

    return console->size;
}

static uint32_t console_head(void)
{
    return (uint32_t)console_length();
}

static size_t console_read(size_t offset, void *buf, size_t length)
{
    size_t size = console_length();
    /* The oldest byte: after a wrap it's the next one to be written */
    size_t oldest = (size == console_capacity) ? console->start : 0;
    uint8_t *out = buf;
    size_t done = 0;

    if (offset >= size)
        return 0;

    length = MIN(length, size - offset);

    while (done < length) {
        size_t pos = (oldest + offset + done) % console_capacity;
        size_t n = MIN(length - done, console_capacity - pos);

        memcpy(&out[done], &console->data[pos], n);
        done += n;
    }

    return done;
}

static const struct log_source linux_log = {
    .name = "linux",
    .description = "Linux's console from ramoops",
    .head = console_head,
    .length = console_length,
    .read = console_read,
};

int linux_ramoops_init(const struct linux_ramoops_config *cfg)
{
    size_t zones = cfg->console_size + cfg->ftrace_size + cfg->pmsg_size;
    size_t dump_size, records, dump_used = 0;

    if (zones > cfg->size || cfg->console_size <= sizeof(struct persistent_ram_buffer))
        return -PB_ERR_PARAM;

    /*
     * The dmesg records come first: as many record_size zones as fit, each
     * an even share of the space (ramoops_init_przs()). Without records the
     * console starts at the region.
     */
    dump_size = cfg->size - zones;
    if (dump_size != 0 && cfg->record_size != 0) {
        records = dump_size / cfg->record_size;
        if (records != 0)
            dump_used = records * ((dump_size / records) & ~(size_t)1);
    }

    console = (const struct persistent_ram_buffer *)(cfg->addr + dump_used);
    console_capacity = cfg->console_size - sizeof(struct persistent_ram_buffer);

    return log_source_register(&linux_log);
}
