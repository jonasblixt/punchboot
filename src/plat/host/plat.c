/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <board_defs.h>
#include <drivers/fuse/test_fuse_bio.h>
#include <errno.h>
#include <pb/console.h>
#include <pb/plat.h>
#include <plat/host/host.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

static const uint8_t device_unique_id[8] = "\xbe\x4e\xfc\xb4\x32\x58\xcd\x63";

/* The fuses' device: one block, the test fuse driver's array */
#define HOST_FUSES_SIZE 512
static const unsigned char *host_fuses_uu = (const unsigned char *)"\xab\x46\x86\x44\xad\xc2\x40"
                                                                   "\x47\xa1\xe3\x1f\x18\xaf\xd5"
                                                                   "\x22\x6a";
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

#ifdef CONFIG_ENABLE_WATCHDOG
/*
 * The watchdog, a fake, for the configuration that has one (F-341): it is
 * armed in plat_early_init() with CONFIG_WATCHDOG_TIMEOUT and counts the
 * kicks. It resets nothing: a hang on the host is a test's timeout. With
 * --watchdog PATH the tests see it, "timeout <s>" and "kicks <n>" written
 * when it is armed and again at the exit, which a reset is.
 */
static unsigned long wdog_kicks;
static bool wdog_armed;

static void wdog_report(void)
{
    FILE *fp;

    if (host_opts.watchdog_path == NULL)
        return;

    fp = fopen(host_opts.watchdog_path, "w");

    if (fp == NULL)
        return;

    fprintf(fp, "timeout %d\nkicks %lu\n", CONFIG_WATCHDOG_TIMEOUT, wdog_kicks);
    fclose(fp);
}

static void wdog_arm(void)
{
    wdog_armed = true;
    wdog_report();
    atexit(wdog_report);
}
#endif

int plat_early_init(void)
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

    /* The memory mapped flash, for partitions that are booted in place */
    if (host_opts.nor_path != NULL &&
        host_nor_map(host_opts.nor_path, BOARD_NOR_BASE, BOARD_NOR_SIZE) != PB_OK)
        return -PB_ERR_MEM;

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

#ifdef CONFIG_ENABLE_WATCHDOG
    /* After the RAM, as fopencookie(): its report is a file */
    wdog_arm();
#endif

    return PB_OK;
}

/*
 * Fuses can be read from here on (platform.md 2.2), as a SoC's are: a file of
 * their own, not on the disk, so that they don't depend on its partition
 * table (F-217). Fuses that can't be read are a missing prerequisite of
 * command mode, and main() resets: with a fuse array of zeros a locked
 * device would read as open.
 */
int plat_init(void)
{
    int rc;
    bio_dev_t fuses =
        host_disk_init(host_opts.fuse_path, HOST_FUSES_SIZE, host_fuses_uu, "Host fuses");

    if (fuses < 0) {
        LOG_ERR("Could not set up the fuses (%i)", fuses);
        return fuses;
    }

    /* Not a device for a host to see or write: the life cycle commands
     * write the fuses */
    rc = bio_set_flags(fuses, 0);

    if (rc == PB_OK)
        rc = test_fuse_init(fuses);

    if (rc != PB_OK)
        LOG_ERR("Could not read the fuses (%i)", rc);

    return rc;
}

/* Command mode's transport is the socket (socket.c): there is no port to
 * set up (platform.md 3.10) */
int plat_cm_init(void)
{
    return PB_OK;
}

void plat_reset(void)
{
    /* exit() rather than _exit(), so that coverage data is written */
    exit(EXIT_SUCCESS);
}

uint32_t plat_get_us_tick(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000000ull + ts.tv_nsec / 1000);
}

/* Without CONFIG_ENABLE_WATCHDOG there is none, as before */
void plat_wdog_kick(void)
{
#ifdef CONFIG_ENABLE_WATCHDOG
    if (wdog_armed)
        wdog_kicks++;
#endif
}

/* A process has no reset status: every start is a power on */
int plat_get_boot_reason(struct pb_boot_reason *reason)
{
    reason->kind = PB_BOOT_REASON_POR;
    reason->raw = 0;
    reason->name = "POR";
    return PB_OK;
}

int plat_get_unique_id(uint8_t *output, size_t *length)
{
    if (sizeof(device_unique_id) > *length)
        return -PB_ERR_BUF_TOO_SMALL;

    memcpy(output, device_unique_id, sizeof(device_unique_id));
    *length = sizeof(device_unique_id);
    return PB_OK;
}

/* A process has no ROM that loaded it: not supported, unless a test of the
 * serial download stop says where pb came from (--boot-source) */
int plat_get_boot_source(struct pb_boot_source *src)
{
    if (!host_opts.boot_source_set)
        return -PB_ERR_NOT_SUPPORTED;

    memset(src, 0, sizeof(*src));
    src->kind = host_opts.boot_source;
    return PB_OK;
}

/* A process has no TrustZone, no watchdog and no ROM, and the crypto backend
 * is the board's: everything is absent (platform.md 3.8). A test of the
 * boot's preconditions asks for a DRAM filter that is bypassed */
void plat_get_status(struct plat_status *status)
{
    status->tz_dram_filter = host_opts.dram_filter_bypassed ? PLAT_TZ_DRAM_BYPASSED
                                                            : PLAT_TZ_DRAM_NONE;
    status->wdog_armed = false;
    status->crypto_engine = false;
    status->rom_log = false;
}

int plat_lockdown(void)
{
    return PB_OK;
}
