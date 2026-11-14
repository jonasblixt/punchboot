/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's life cycle and keys (command-mode.md 5.2).
 */

#include "cm_internal.h"
#include <pb/rot.h>
#include <pb/slc.h>

int cm_cmd_slc_read(void)
{
    int rc;
    struct pb_result_slc slc_status = { 0 };
    struct pb_result_slc_key_status key_status = { 0 };
    cm_slc = slc_read_status();

    if (cm_slc < 0) {
        return cm_slc;
    } else {
        slc_status.slc = cm_slc;
        rc = PB_OK;
    }
    pb_wire_init_result2(&cm_result, cm_error_to_wire(rc), &slc_status, sizeof(slc_status));

    cm_write(&cm_result, sizeof(cm_result));

    if (rc < 0)
        return rc;

    /* The wire has 16 slots for each list (command-mode.md 5.2): a keystore
     * with more keys doesn't write past them */
    size_t no_of_keys = 0;

    /* Without a RoT: empty lists, and its error as the result (F-346) */
    rc = rot_no_of_keys(&no_of_keys);

    if (no_of_keys > ARRAY_SIZE(key_status.active)) {
        LOG_ERR("The keystore has more keys than the %zu that are listed",
                ARRAY_SIZE(key_status.active));
        no_of_keys = ARRAY_SIZE(key_status.active);
    }

    for (size_t i = 0; i < no_of_keys; i++) {
        key_id_t key_id = 0;
        int key_rc = rot_key_idx_to_id(i, &key_id);

        if (key_rc == PB_OK)
            key_rc = rot_read_key_status_by_idx(i);

        if (key_rc == PB_OK)
            key_status.active[i] = key_id;
        else if (key_rc == -PB_ERR_KEY_REVOKED)
            key_status.revoked[i] = key_id;
        else if (rc == PB_OK)
            rc = key_rc;
        LOG_INFO("Key 0x%x status=%i", key_id, key_rc);
    }

    cm_write(&key_status, sizeof(key_status));

    /* A revoked key is in the key status, the result is only for errors
     * reading it. It used to be the last key's status (F-072). */
    return rc;
}

int cm_cmd_slc_set_configuration(void)
{
    int rc;

    LOG_DBG("Set configuration");
    rc = slc_set_configuration();
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    cm_slc = slc_read_status();

    return rc;
}

int cm_cmd_slc_set_configuration_lock(void)
{
    int rc;

    LOG_DBG("Set configuration lock");
    rc = slc_set_configuration_locked();
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    cm_slc = slc_read_status();

    return rc;
}

int cm_cmd_slc_set_eol(void)
{
    int rc;

    LOG_DBG("Set EOL");
#ifdef CONFIG_SLC_EOL
    rc = slc_set_eol();
#else
    /* EOL without the product's option (system.md 4): the build left
     * it out, which is NOT_SUPPORTED and never INVALID_COMMAND
     * (command-mode.md 4.5, 5.2) */
    rc = -PB_ERR_NOT_SUPPORTED;
#endif
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    cm_slc = slc_read_status();

    return rc;
}

int cm_cmd_slc_revoke_key(void)
{
    int rc = PB_OK;

    struct pb_command_revoke_key *revoke_cmd = (struct pb_command_revoke_key *)cm_cmd.request;

    LOG_DBG("Revoke key %x", revoke_cmd->key_id);

    rc = rot_revoke_key(revoke_cmd->key_id);
    pb_wire_init_result(&cm_result, cm_error_to_wire(rc));

    return rc;
}
