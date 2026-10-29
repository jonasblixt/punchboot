/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/boot.h>
#include <boot/linux_ramoops.h>
#include <pb/arch.h>
#include <pb/cm.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/ramlog.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef CONFIG_BOOT_LINUX_RAMOOPS
static const struct linux_ramoops_config ramoops = {
    .addr = CONFIG_BOOT_LINUX_RAMOOPS_ADDR,
    .size = CONFIG_BOOT_LINUX_RAMOOPS_SIZE,
    .record_size = CONFIG_BOOT_LINUX_RAMOOPS_RECORD_SIZE,
    .console_size = CONFIG_BOOT_LINUX_RAMOOPS_CONSOLE_SIZE,
    .ftrace_size = CONFIG_BOOT_LINUX_RAMOOPS_FTRACE_SIZE,
    .pmsg_size = CONFIG_BOOT_LINUX_RAMOOPS_PMSG_SIZE,
};
#endif

void main(void)
{
    int rc;

#ifdef CONFIG_RAMLOG
    /* First, so that prints from before console_init() are kept and written
     * to the UART once it's set up */
    ramlog_init();
#endif
    arch_init();
#ifdef CONFIG_BOOT_LINUX_RAMOOPS
    /* Only reads the region when the log is read, after the DRAM init */
    if (linux_ramoops_init(&ramoops) != PB_OK)
        printf("Bad ramoops config\n\r");
#endif

    rc = plat_init();

    if (rc != PB_OK)
        goto enter_command_mode;

    rc = plat_board_init();

    if (rc != PB_OK)
        goto enter_command_mode;

    rc = boot(NULL); /* Normally we would not return from this function */

enter_command_mode:
    printf("Boot aborted (%i), entering command mode\n\r", rc);
#ifdef CONFIG_CM
    cm_run();
#endif
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
