/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A copy of the console output in a ring buffer in RAM, for boards without
 * a UART and for reading the log after the fact.
 */

#include <arch/arch.h>
#include <pb/pb.h>
#include <pb/ramlog.h>
#include <string.h>

#define RAMLOG_SIZE (CONFIG_RAMLOG_SIZE_KiB * 1024)

/* In .ramlog, so that it isn't zeroed by the startup code and is found by
 * its symbol name. Cache line aligned, so that syncing the header and the
 * data never touches anything else. */
static struct {
    struct pb_ramlog_hdr hdr;
    char data[RAMLOG_SIZE];
} ramlog __section(".ramlog") __aligned(64) __used;

/* Set by ramlog_init(). In the SPL the log is in DRAM, which may not be
 * set up yet: nothing may touch it before ramlog_init(). */
static bool ramlog_active;

static void ramlog_sync(const void *p, size_t length)
{
#ifdef CONFIG_RAMLOG_SYNC
    arch_clean_cache_range((uintptr_t)p, length);
#else
    (void)p;
    (void)length;
#endif
}

void ramlog_init(void)
{
    ramlog.hdr.magic = PB_RAMLOG_MAGIC;
    ramlog.hdr.version = PB_RAMLOG_VERSION;
    ramlog.hdr.size = RAMLOG_SIZE;
    ramlog.hdr.head = 0;
    ramlog_sync(&ramlog.hdr, sizeof(ramlog.hdr));
    ramlog_active = true;
}

void ramlog_putc(char c)
{
    char *p;

    if (!ramlog_active || ramlog.hdr.magic != PB_RAMLOG_MAGIC)
        return;

    /* The data before the head, so that a reader never sees a head that
     * counts a character which isn't in RAM yet */
    p = &ramlog.data[ramlog.hdr.head % RAMLOG_SIZE];
    *p = c;
    ramlog_sync(p, 1);
    ramlog.hdr.head++;
    ramlog_sync(&ramlog.hdr, sizeof(ramlog.hdr));
}

size_t ramlog_length(void)
{
    if (!ramlog_active || ramlog.hdr.magic != PB_RAMLOG_MAGIC)
        return 0;

    return MIN(ramlog.hdr.head, (uint32_t)RAMLOG_SIZE);
}

uint32_t ramlog_head(void)
{
    if (!ramlog_active || ramlog.hdr.magic != PB_RAMLOG_MAGIC)
        return 0;

    return ramlog.hdr.head;
}

size_t ramlog_read(size_t offset, void *buf, size_t length)
{
    size_t avail = ramlog_length();
    size_t oldest;
    size_t copied = 0;
    char *out = buf;

    if (offset >= avail)
        return 0;

    length = MIN(length, avail - offset);
    oldest = (ramlog.hdr.head - avail) % RAMLOG_SIZE;

    while (copied < length) {
        size_t pos = (oldest + offset + copied) % RAMLOG_SIZE;
        size_t chunk = MIN(length - copied, RAMLOG_SIZE - pos);

        memcpy(&out[copied], &ramlog.data[pos], chunk);
        copied += chunk;
    }

    return copied;
}
