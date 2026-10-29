/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb-tools/wire.h>
#include <pb/crc.h>
#include <pb/log_source.h>
#include <pb/pb.h>
#include <pb/ramlog.h>
#include <stdio.h>
#include <string.h>

/* Logs that platforms and boards register */
#define LOG_SOURCES_MAX 4
/* The list of logs, one line each */
#define LOG_LIST_SIZE   512

static const struct log_source *sources[LOG_SOURCES_MAX];
static char list_text[LOG_LIST_SIZE];

#ifdef CONFIG_RAMLOG
static const struct log_source pb_log = {
    .name = "pb",
    .description = "pb's console output",
    .head = ramlog_head,
    .length = ramlog_length,
    .read = ramlog_read,
};
#endif

static void list_add(size_t *pos, const struct log_source *src)
{
    size_t left = sizeof(list_text) - *pos;

    /* Name, space, description, newline */
    if (strlen(src->name) + strlen(src->description) + 2 >= left)
        return;

    *pos += snprintf(&list_text[*pos], left, "%s %s\n", src->name, src->description);
}

/* The list is short, so it's made again for each call */
static size_t list_render(void)
{
    size_t pos = 0;

#ifdef CONFIG_RAMLOG
    list_add(&pos, &pb_log);
#endif
    for (size_t i = 0; i < ARRAY_SIZE(sources); i++) {
        if (sources[i] != NULL)
            list_add(&pos, sources[i]);
    }

    return pos;
}

static uint32_t list_head(void)
{
    return (uint32_t)list_render();
}

static size_t list_length(void)
{
    return list_render();
}

static size_t list_read(size_t offset, void *buf, size_t length)
{
    size_t size = list_render();

    if (offset >= size)
        return 0;

    length = MIN(length, size - offset);
    memcpy(buf, &list_text[offset], length);
    return length;
}

static const struct log_source list_log = {
    .name = "list",
    .description = "The logs this device has",
    .head = list_head,
    .length = list_length,
    .read = list_read,
};

/* pb_id() and bpak_id(): CRC32 of the name, without the inversions */
static uint32_t name_id(const char *name)
{
    return crc32(0, (const uint8_t *)name, (uint32_t)strlen(name));
}

int log_source_register(const struct log_source *src)
{
    for (size_t i = 0; i < ARRAY_SIZE(sources); i++) {
        if (sources[i] == src)
            return PB_OK;
    }

    for (size_t i = 0; i < ARRAY_SIZE(sources); i++) {
        if (sources[i] == NULL) {
            sources[i] = src;
            return PB_OK;
        }
    }

    return -PB_ERR_MEM;
}

const struct log_source *log_source_get(uint32_t id)
{
    if (id == 0)
        id = PB_LOG_ID_PB;

#ifdef CONFIG_RAMLOG
    if (id == PB_LOG_ID_PB)
        return &pb_log;
#endif
    if (id == PB_LOG_ID_LIST)
        return &list_log;

    for (size_t i = 0; i < ARRAY_SIZE(sources); i++) {
        if (sources[i] != NULL && name_id(sources[i]->name) == id)
            return sources[i];
    }

    return NULL;
}
