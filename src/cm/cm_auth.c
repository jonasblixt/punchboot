/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode's authentication (command-mode.md 3.3): the token and the
 * password, and the delay of a failed attempt.
 */

#include "cm_internal.h"
#include <pb/crypto.h>
#include <pb/delay.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <uuid.h>

#ifdef CONFIG_CM_AUTH_TOKEN
static int auth_token(uint32_t key_id, uint8_t *sig, size_t size)
{
    int rc = -PB_ERR;
    bool verified = false;
    char device_uu_str[37];
    const uint8_t *key_der_data = NULL;
    size_t key_der_data_length = 0;
    hash_t hash_kind = 0;
    dsa_t dsa_kind = 0;

    rc = rot_read_key_status(key_id);

    if (rc != PB_OK)
        return rc;

    rc = rot_get_dsa_key(key_id, &dsa_kind, &key_der_data, &key_der_data_length);

    if (rc != PB_OK)
        return rc;

    uuid_unparse(cm_device_uu, device_uu_str);

    switch (dsa_kind) {
    case DSA_EC_SECP256r1:
        hash_kind = HASH_SHA256;
        break;
    case DSA_EC_SECP384r1:
        hash_kind = HASH_SHA384;
        break;
    case DSA_EC_SECP521r1:
        hash_kind = HASH_SHA512;
        break;
    default:
        return -PB_ERR_NOT_SUPPORTED;
    }

    rc = hash_init(hash_kind);

    if (rc != PB_OK)
        return rc;

    rc = hash_update(device_uu_str, 36);

    if (rc != PB_OK)
        return rc;

    rc = hash_final(cm_hash, sizeof(cm_hash));

    if (rc != PB_OK)
        return rc;

    rc = dsa_verify(dsa_kind,
                    sig,
                    size,
                    key_der_data,
                    key_der_data_length,
                    hash_kind,
                    cm_hash,
                    sizeof(cm_hash),
                    &verified);

    if (rc == 0 && verified) {
        LOG_INFO("Authentication successful");
        return PB_OK;
    } else {
        LOG_ERR("Authentication failed (%i) %i", rc, verified);
        return -PB_ERR_AUTHENTICATION_FAILED;
    }

    return -PB_ERR_AUTHENTICATION_FAILED;
}
#endif

#if CONFIG_CM_AUTH_PASSWORD || CONFIG_CM_AUTH_TOKEN
/* Hosts send the authentication data after an OK result, and check that
 * result first. A bad size is refused before the data and sets *refused:
 * nothing was tried, so the session stays as it was (command-mode.md 3.3). */
static int auth_read_data(const struct pb_command_authenticate *auth_cmd, bool *refused)
{
    *refused = false;

    if (auth_cmd->size == 0 || auth_cmd->size > CM_DATA_XFER_SIZE) {
        LOG_ERR("Bad authentication data size: %u", auth_cmd->size);
        *refused = true;
        return -PB_ERR_PARAM;
    }

    pb_wire_init_result(&cm_result, PB_RESULT_OK);
    cm_write(&cm_result, sizeof(cm_result));

    return cm_read(cm_buffer[0], CM_DATA_XFER_SIZE);
}

#define CM_AUTH_FAILED_DELAY_US 1000000
#define CM_AUTH_FAILED_KICK_US  100000

/* The result of a failed attempt is sent 1 s after the data arrived, with
 * every method: the check's own time is part of it (command-mode.md 3.3). No
 * counter, no lockout. The tick wraps, so only differences count (plat.h).
 * pb_delay_*() and pb_timeout_*() never kick (platform.md 3.3), this does:
 * at once, after the check, and every 100 ms (the shortest watchdog is 5 s).
 * The transport isn't touched: a host that leaves gets nothing sooner. */
static void auth_failed_wait(unsigned int arrived)
{
    unsigned int now = plat_get_us_tick();
    unsigned int kicked = now;

    plat_wdog_kick();

    while ((now - arrived) < CM_AUTH_FAILED_DELAY_US) {
        if ((now - kicked) >= CM_AUTH_FAILED_KICK_US) {
            plat_wdog_kick();
            kicked = now;
        }

        now = plat_get_us_tick();
    }
}
#endif

int cm_cmd_auth(void)
{
    int rc = -PB_ERR_NOT_IMPLEMENTED;

#if CONFIG_CM_AUTH_PASSWORD || CONFIG_CM_AUTH_TOKEN
    struct pb_command_authenticate *auth_cmd = (struct pb_command_authenticate *)cm_cmd.request;
    bool refused;
#endif
    /* A method that isn't in the build, or a board without a password
     * check, is refused before the data and changes nothing */
    pb_wire_init_result(&cm_result, -PB_RESULT_NOT_SUPPORTED);

#ifdef CONFIG_CM_AUTH_TOKEN
    if (auth_cmd->method == PB_AUTH_ASYM_TOKEN) {
        rc = auth_read_data(auth_cmd, &refused);

        if (rc == PB_OK) {
            unsigned int arrived = plat_get_us_tick();

            rc = auth_token(auth_cmd->key_id, cm_buffer[0], auth_cmd->size);

            if (rc != PB_OK)
                auth_failed_wait(arrived);
        }

        if (rc == PB_OK)
            cm_authenticated = true;
        else if (!refused)
            cm_authenticated = false;

        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    }
#endif

#if CONFIG_CM_AUTH_PASSWORD
    if (auth_cmd->method == PB_AUTH_PASSWORD && cm_cfg->password_auth) {
        rc = auth_read_data(auth_cmd, &refused);

        if (rc == PB_OK) {
            unsigned int arrived = plat_get_us_tick();

            rc = cm_cfg->password_auth((char *)cm_buffer[0], auth_cmd->size);

            if (rc != PB_OK)
                auth_failed_wait(arrived);
        }

        if (rc == PB_OK)
            cm_authenticated = true;
        else if (!refused)
            cm_authenticated = false;

        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    }
#endif

    return rc;
}
