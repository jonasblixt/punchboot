/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/command.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/plat_info.h>
#include <stdio.h>
#include <string.h>

/* The specifics: a few lines (the 8X has three) */
#define EXTRA_SIZE 512
/* The first line and the specifics */
#define TEXT_SIZE  (EXTRA_SIZE + 128)

static char extra[EXTRA_SIZE];
static char text[TEXT_SIZE];

/* "1200 MHz", or what 0 means */
static void put_mhz(char *buf, size_t size, uint32_t hz, const char *zero)
{
    if (hz == 0)
        (void)snprintf(buf, size, "%s", zero);
    else
        (void)snprintf(buf, size, "%u MHz", (unsigned int)(hz / 1000000));
}

int plat_info_text(char *out, size_t size)
{
    struct pb_plat_info info = { 0 };
    char cpu[16];
    char mem[16];
    int rc;
    int n;

    if (size == 0)
        return -PB_ERR_BUF_TOO_SMALL;

    extra[0] = '\0';
    rc = plat_get_info(&info, extra, sizeof(extra));
    if (rc != PB_OK) {
        out[0] = '\0';
        return rc;
    }
    extra[sizeof(extra) - 1] = '\0';

    put_mhz(cpu, sizeof(cpu), info.cpu_hz, "unknown");
    put_mhz(mem, sizeof(mem), info.mem_hz, "none set up");

    n = snprintf(out,
                 size,
                 "SoC %s rev %s, CPU %s, memory %s\n%s",
                 info.soc ? info.soc : "unknown",
                 info.rev ? info.rev : "unknown",
                 cpu,
                 mem,
                 extra);
    if (n < 0)
        return -PB_ERR;

    return ((size_t)n < size) ? n : (int)(size - 1);
}

void plat_info_log(void)
{
    char *line;
    int rc = plat_info_text(text, sizeof(text));

    if (rc < 0) {
        LOG_ERR("No SoC information (%i)", rc);
        return;
    }

    /* A log line each */
    line = text;
    while (*line != '\0') {
        char *end = strchr(line, '\n');

        if (end != NULL)
            *end = '\0';
        LOG_INFO("%s", line);
        if (end == NULL)
            break;
        line = end + 1;
    }
}

/* The named command 'info' (command-mode.md 5.6): the text and its NUL */
static int
cmd_info(const uint8_t *request, size_t request_size, uint8_t *response, size_t *response_size)
{
    size_t room = *response_size;
    int rc;

    (void)request;
    (void)request_size;

    *response_size = 0;
    if (room == 0)
        return -PB_ERR_BUF_TOO_SMALL;

    rc = plat_info_text((char *)response, room);
    if (rc < 0)
        return rc;

    *response_size = (size_t)rc + 1;
    return PB_OK;
}

DEFINE_COMMAND("info", "The SoC, its clocks and its firmware", cmd_info);
