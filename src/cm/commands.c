/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The named commands' list (doc/architecture/command-mode.md 5.6, D-076)
 */

#include <pb/command.h>
#include <pb/crc.h>
#include <pb/pb.h>
#include <string.h>

/* pb_id() and bpak_id(): CRC32 of the name, without the inversions */
static uint32_t command_id(const char *name)
{
    return crc32(0, (const uint8_t *)name, (uint32_t)strlen(name));
}

/* 1 to 31 characters of [a-z0-9-] (command-mode.md 5.6). Not read beyond the
 * NUL that must come within 32 bytes */
static bool name_is_valid(const char *name)
{
    if (name == NULL)
        return false;

    size_t i;

    for (i = 0; i <= CM_COMMAND_NAME_MAX && name[i] != '\0'; i++) {
        char c = name[i];

        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }

    return i >= 1 && i <= CM_COMMAND_NAME_MAX && name[i] == '\0';
}

/* At most 79 printable characters */
static bool help_is_valid(const char *help)
{
    if (help == NULL)
        return false;

    size_t i;

    for (i = 0; i <= CM_COMMAND_HELP_MAX && help[i] != '\0'; i++) {
        if (help[i] < 0x20 || help[i] > 0x7e)
            return false;
    }

    return i <= CM_COMMAND_HELP_MAX && help[i] == '\0';
}

int cm_commands_check(const struct cm_command *list, size_t count, size_t room)
{
    /* An entry on the wire is 128 bytes (wire.h) */
    if (count > room / 128) {
        LOG_ERR("Commands: %zu, room for %zu", count, room / 128);
        return -PB_ERR_PARAM;
    }

    for (size_t i = 0; i < count; i++) {
        const struct cm_command *c = &list[i];

        if (!name_is_valid(c->name) || strcmp(c->name, "list") == 0) {
            LOG_ERR("Command %zu: the name isn't 1 to %u of [a-z0-9-], or is 'list'",
                    i,
                    (unsigned int)CM_COMMAND_NAME_MAX);
            return -PB_ERR_PARAM;
        }

        if (!help_is_valid(c->help) || c->run == NULL) {
            LOG_ERR("Command '%s': no function, or no help of at most %u printable characters",
                    c->name,
                    (unsigned int)CM_COMMAND_HELP_MAX);
            return -PB_ERR_PARAM;
        }

        uint32_t id = command_id(c->name);

        for (size_t j = 0; j < i; j++) {
            if (command_id(list[j].name) == id) {
                LOG_ERR("Commands '%s' and '%s': one id", list[j].name, c->name);
                return -PB_ERR_PARAM;
            }
        }
    }

    return PB_OK;
}

const struct cm_command *cm_commands(size_t *count)
{
    const struct cm_command *start = __start_pb_commands;
    const struct cm_command *stop = __stop_pb_commands;

    *count = (start < stop) ? (size_t)(stop - start) : 0;

    return start;
}

const struct cm_command *cm_command_find(uint32_t id)
{
    size_t count;
    const struct cm_command *list = cm_commands(&count);

    for (size_t i = 0; i < count; i++) {
        if (command_id(list[i].name) == id)
            return &list[i];
    }

    return NULL;
}
