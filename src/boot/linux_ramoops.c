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
 * position of the next write and the bytes used, then the data. A dmesg
 * record is the kernel log Linux saved when it dumped it (ramoops_pstore_write():
 * an oops or panic, and a reboot with max-reason 4). The newest record is
 * the log "linux"; a compressed record can't be read.
 */

#include <boot/linux_ramoops.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <string.h>

#define PERSISTENT_RAM_SIG 0x43474244 /* "DBGC" */
/* A dmesg record starts with "====<sec>.<usec>-D\n" ('C' if compressed) */
#define KMSG_HDR           "===="

struct persistent_ram_buffer {
    uint32_t sig;
    uint32_t start; /* Where the next byte goes */
    uint32_t size; /* Bytes in the buffer, up to its size */
    uint8_t data[];
};

static uintptr_t records;
static size_t record_count;
static size_t record_zone;

/* Digits at 'pos', at most 'max' of them: false if there are none */
static bool parse_digits(const uint8_t *text, size_t size, size_t *pos, size_t max, uint64_t *value)
{
    size_t start = *pos;

    *value = 0;
    while (*pos < size && *pos - start < max && text[*pos] >= '0' && text[*pos] <= '9')
        *value = *value * 10 + (text[(*pos)++] - '0');

    return *pos != start;
}

static const struct persistent_ram_buffer *record_zone_at(size_t index)
{
    return (const struct persistent_ram_buffer *)(records + index * record_zone);
}

/*
 * A dmesg record's time (in us) and size. False for a zone without a record
 * that can be read: no valid header, empty or compressed. Linux zaps a zone
 * before it writes a record and cuts the record to fit, so the record runs
 * from the start of the zone's data.
 */
static bool record_parse(size_t index, uint64_t *time, size_t *size)
{
    const struct persistent_ram_buffer *zone = record_zone_at(index);
    size_t capacity = record_zone - sizeof(struct persistent_ram_buffer);
    const uint8_t *data = zone->data;
    uint64_t sec, usec;
    size_t pos = sizeof(KMSG_HDR) - 1;

    if (zone->sig != PERSISTENT_RAM_SIG || zone->size > capacity)
        return false;

    *size = zone->size;
    if (*size < pos || memcmp(data, KMSG_HDR, pos) != 0)
        return false;
    if (!parse_digits(data, *size, &pos, 20, &sec) || pos >= *size || data[pos++] != '.')
        return false;
    if (!parse_digits(data, *size, &pos, 9, &usec) || pos >= *size)
        return false;
    /* Before Linux 4.x the header had no "-D" */
    if (data[pos] == '-') {
        if (pos + 1 >= *size || data[pos + 1] != 'D')
            return false;
        pos += 2;
    }
    if (pos >= *size || data[pos] != '\n')
        return false;

    *time = sec * 1000000 + usec;
    return true;
}

/* The newest record, with its header, NULL if there is none */
static const uint8_t *newest_record(size_t *length)
{
    const uint8_t *newest = NULL;
    uint64_t newest_time = 0;

    for (size_t i = 0; i < record_count; i++) {
        uint64_t time;
        size_t size;

        if (!record_parse(i, &time, &size))
            continue;
        if (newest == NULL || time > newest_time) {
            newest = record_zone_at(i)->data;
            newest_time = time;
            *length = size;
        }
    }

    return newest;
}

static size_t linux_length(void)
{
    size_t length = 0;

    (void)newest_record(&length);
    return length;
}

static uint32_t linux_head(void)
{
    return (uint32_t)linux_length();
}

static size_t linux_read(size_t offset, void *buf, size_t length)
{
    size_t size = 0;
    const uint8_t *text = newest_record(&size);

    if (text == NULL || offset >= size)
        return 0;

    length = MIN(length, size - offset);
    memcpy(buf, &text[offset], length);
    return length;
}

static const struct log_source linux_log = {
    .name = "linux",
    .description = "Linux's kernel log at its last panic, oops or reboot, from ramoops",
    .head = linux_head,
    .length = linux_length,
    .read = linux_read,
};

int linux_ramoops_init(const struct linux_ramoops_config *cfg)
{
    size_t zones = cfg->console_size + cfg->ftrace_size + cfg->pmsg_size;
    size_t dump_size;

    if (zones > cfg->size || cfg->record_size == 0)
        return -PB_ERR_PARAM;

    /*
     * The dmesg records come first: as many record_size zones as fit, each
     * an even share of the space (ramoops_init_przs())
     */
    dump_size = cfg->size - zones;
    record_count = dump_size / cfg->record_size;
    if (record_count == 0)
        return -PB_ERR_PARAM;

    record_zone = (dump_size / record_count) & ~(size_t)1;
    /* A zone too small for its header can't hold a record */
    if (record_zone <= sizeof(struct persistent_ram_buffer))
        return -PB_ERR_PARAM;

    records = cfg->addr;

    return log_source_register(&linux_log);
}
