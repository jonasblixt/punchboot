/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's boot commands (command-mode.md 5.5): BOOT_BPAK, BOOT_PART,
 * and the boot source policy's partition.
 */

#include "cm_internal.h"
#include <boot/boot.h>
#include <boot/policy.h>
#include <bpak/bpak.h>

/*
 * The end of both boot commands, after the host's last OK (command-mode.md
 * 5.5, board.md 5.4): the transport is disconnected, then late_boot(),
 * plat_lockdown() and the jump. The session is over whatever follows, so a
 * boot that returns here, stopped or failed, leaves command mode and pb
 * resets (2.3): nothing is read from the transport that is gone.
 */
static int cm_boot_jump(void)
{
    int rc;

    cm_disconnect();

    rc = boot_jump();

    LOG_ERR("The boot returned after the disconnect (%i), resetting", rc);
    cm_leave_requested = true;

    return (rc != PB_OK) ? rc : -PB_ERR;
}

/* Set when the boot has read from the host: the header comes first */
static bool bpak_boot_read_any;

static int bpak_boot_read_f(int64_t offset, size_t length, void *buf)
{
    (void)offset;
    bpak_boot_read_any = true;
    return cm_read(buf, length);
}

static int bpak_boot_result_f(int rc)
{
    LOG_DBG("%i", rc);
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    return cm_write(&cm_result, sizeof(cm_result));
}

int cm_cmd_boot_bpak(void)
{
    int rc;
    struct pb_command_bpak_boot *bpak_boot_cmd = (struct pb_command_bpak_boot *)&cm_cmd.request;

    boot_clear_set_flags(0, BOOT_FLAG_CMD | (bpak_boot_cmd->verbose ? BOOT_FLAG_VERBOSE : 0));

    pb_wire_init_result(&cm_result, PB_RESULT_OK);
    rc = cm_write(&cm_result, sizeof(cm_result));

    if (rc != PB_OK)
        return rc;

    boot_set_source(BOOT_SOURCE_CB);
    boot_configure_load_cb(bpak_boot_read_f, bpak_boot_result_f);

    bpak_boot_read_any = false;
    rc = boot_load(bpak_boot_cmd->uuid);

    if (rc != PB_OK) {
        /* Refused before the header was read (a precondition, early_boot()'s
         * stop, no handover): the host sends it after the OK all the same,
         * so it is read and its result carries the refusal (command-mode.md
         * 5.5, D-058) */
        if (!bpak_boot_read_any && cm_drain(sizeof(struct bpak_header)) == PB_OK)
            bpak_boot_result_f(rc);
        return rc;
    }

    return cm_boot_jump();
}

/* PART_ACTIVATE: the partition the boot source policy boots */
int cm_cmd_part_activate(void)
{
    int rc = PB_OK;

    struct pb_command_activate_part *activate_cmd =
        (struct pb_command_activate_part *)cm_cmd.request;
    if (cm_part_hidden(activate_cmd->uuid))
        rc = -PB_ERR_NOT_FOUND;
    else
        rc = boot_set_boot_partition(activate_cmd->uuid);
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));

    return rc;
}

/* BOOT_STATUS: the partition the policy boots */
int cm_cmd_boot_status(void)
{
    int rc = PB_OK;

    struct pb_result_boot_status boot_result = { 0 };
    boot_get_boot_partition(boot_result.uuid);

    pb_wire_init_result2(&cm_result, cm_error_to_wire(rc), &boot_result, sizeof(boot_result));
    rc = PB_RESULT_OK;

    return rc;
}

/* BOOT_PART: the boot from a partition, which sends its result itself */
int cm_cmd_boot_part(void)
{
    int rc;

    struct pb_command_boot_part *boot_cmd = (struct pb_command_boot_part *)cm_cmd.request;

    if (cm_part_hidden(boot_cmd->uuid)) {
        rc = -PB_ERR_NOT_FOUND;
    } else {
        boot_set_source(BOOT_SOURCE_BIO);
        boot_clear_set_flags(0, BOOT_FLAG_CMD | (boot_cmd->verbose ? BOOT_FLAG_VERBOSE : 0));
        rc = boot_load(boot_cmd->uuid);
    }

    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    cm_write(&cm_result, sizeof(cm_result));

    /* Only disconnect when booting, the task reads the next command
     * from the transport after an error */
    if (rc == PB_OK)
        rc = cm_boot_jump();

    return rc;
}
