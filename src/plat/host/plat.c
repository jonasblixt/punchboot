/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <board_defs.h>
#include <errno.h>
#include <pb/console.h>
#include <pb/plat.h>
#include <plat/host/host.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

int board_init(void);

static const uint8_t device_unique_id[8] = "\xbe\x4e\xfc\xb4\x32\x58\xcd\x63";
const char *platform_ns_uuid = "\x3f\xaf\xc6\xd3\xc3\x42\x4e\xdf\xa5\xa6\x0e\xb1\x39\xa7\x83\xb5";

/* The process's real stdout, the "UART" */
static FILE *host_stdout;

static void host_console_putc(uintptr_t base, char c)
{
    (void)base;
    fputc(c, host_stdout);
}

static const struct console_ops host_console_ops = {
    .putc = host_console_putc,
};

/* stdout goes through console_putc(), like the UART output on target, so it
 * also ends up in the RAM log */
static ssize_t host_console_write(void *cookie, const char *buf, size_t size)
{
    (void)cookie;

    for (size_t i = 0; i < size; i++)
        console_putc(buf[i]);

    return (ssize_t)size;
}

int plat_init(void)
{
    static const cookie_io_functions_t console_io = {
        .write = host_console_write,
    };
    FILE *console;

    log_time_start();

    /* Unbuffered output, so the log interleaves correctly with the test
     * harness when the process exits on reset. */
    setvbuf(stdout, NULL, _IONBF, 0);

    /* Emulate the board RAM at its real address, images are loaded to the
     * addresses given by their 'pb-load-addr' meta data. */
    void *ram = mmap((void *)BOARD_RAM_BASE,
                     BOARD_RAM_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE | MAP_NORESERVE,
                     -1,
                     0);

    if (ram != (void *)BOARD_RAM_BASE) {
        LOG_ERR("Could not map RAM at 0x%lx (%s)", (unsigned long)BOARD_RAM_BASE, strerror(errno));
        return -PB_ERR_MEM;
    }

    /* After the RAM: fopencookie() is the first malloc(), which sets up the
     * brk heap. Its start is randomized over 1 GiB on x86-64, so a heap set
     * up before could be where the RAM has to go. */
    console = fopencookie(NULL, "w", console_io);

    if (console == NULL) {
        LOG_ERR("Could not open the console stream (%s)", strerror(errno));
        return -PB_ERR_IO;
    }

    setvbuf(console, NULL, _IONBF, 0);
    host_stdout = stdout;
    stdout = console;
    console_init(0, &host_console_ops);

    return PB_OK;
}

int plat_board_init(void)
{
    return board_init();
}

void plat_reset(void)
{
    /* exit() rather than _exit(), so that coverage data is written */
    exit(EXIT_SUCCESS);
}

unsigned int plat_get_us_tick(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000000ull + ts.tv_nsec / 1000);
}

void plat_wdog_kick(void)
{
}

int plat_boot_reason(void)
{
    return 0;
}

const char *plat_boot_reason_str(void)
{
    return "";
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    if (sizeof(device_unique_id) > *length)
        return -PB_ERR_BUF_TOO_SMALL;

    memcpy(output, device_unique_id, sizeof(device_unique_id));
    *length = sizeof(device_unique_id);
    return PB_OK;
}

int plat_get_boot_source(struct pb_boot_source *src)
{
    (void)src;
    return -PB_ERR_NOT_SUPPORTED;
}
