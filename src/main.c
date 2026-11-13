/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/boot.h>
#include <pb/arch.h>
#include <pb/board.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/ramlog.h>
#include <pb/task.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Stopped (system.md 2): the build's tasks, one after the other, again and
 * again, until one returns an error (board.md 4.5). Command mode is one of
 * them, in a build that has it. The watchdog is kicked once per round, and
 * no task kicks it. A build without a task returns at once.
 */
static void run_tasks(void)
{
    bool ran;

    do {
        ran = false;
        plat_wdog_kick();

        for (const struct pb_task *task = __start_pb_tasks; task < __stop_pb_tasks; task++) {
            int rc = task->run();

            ran = true;

            if (rc != PB_OK) {
                LOG_INFO("Task %s ended (%i)", task->name, rc);
                return;
            }
        }
    } while (ran);
}

void main(void)
{
    int rc;

#ifdef CONFIG_RAMLOG
    /* First, so that prints from before console_init() are kept and written
     * to the UART once it's set up */
    ramlog_init();
#endif
    arch_init();

    rc = plat_early_init();

    if (rc != PB_OK)
        goto platform_failed;

    if (board.console_init)
        board.console_init();

    rc = plat_init();

    if (rc != PB_OK)
        goto platform_failed;

    rc = board_setup();

    if (rc != PB_OK)
        goto enter_command_mode;

    rc = boot(NULL); /* Normally we would not return from this function */

enter_command_mode:
    printf("Boot aborted (%i), entering command mode\n\r", rc);
    run_tasks();
    plat_reset();

platform_failed:
    /* Not stopped (platform.md 2): without what the platform sets up there is
     * no console or tick, no MMU, no fuses or no DRAM for command mode's
     * buffers. Neither the boot nor a task runs. After plat_early_init() the
     * line below is in the RAM log only */
    LOG_ERR("Platform init failed (%i), resetting", rc);
    plat_reset();
}

#ifndef CONFIG_ARCH_HOST
/* The host build links against the system libc, which provides exit() */
void exit(int reason)
{
    LOG_ERR("reason = %i", reason);
    plat_reset();
}
#endif
