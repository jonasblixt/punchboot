#include "api.h"
#include <pb-tools/wire.h>
#include <stdlib.h>
#include <string.h>

int pb_api_slc_read(struct pb_context *ctx,
                    uint8_t *slc,
                    uint8_t *active_keys,
                    uint8_t *revoked_keys)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;
    struct pb_result_slc slc_result;
    struct pb_result_slc_key_status key_status;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_SLC_READ);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK) {
        return rc;
    }

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    if (result.result_code != PB_RESULT_OK)
        return result.result_code;

    memcpy(&slc_result, result.response, sizeof(slc_result));

    ctx->d(ctx, 2, "%s: SLC: %i \n", __func__, slc_result.slc);

    (*slc) = slc_result.slc;

    rc = ctx->read(ctx, &key_status, sizeof(key_status));

    if (rc != PB_RESULT_OK)
        return rc;

    if (active_keys != NULL) {
        memcpy(active_keys, key_status.active, sizeof(key_status.active));
    }

    if (revoked_keys != NULL) {
        memcpy(revoked_keys, key_status.revoked, sizeof(key_status.revoked));
    }

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK) {
        return rc;
    }

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_slc_set_configuration(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_SLC_SET_CONFIGURATION);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));

    return result.result_code;
}

int pb_api_slc_set_configuration_lock(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_SLC_SET_CONFIGURATION_LOCK);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_slc_set_end_of_life(struct pb_context *ctx)
{
    int rc;
    struct pb_command cmd;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    pb_wire_init_command(&cmd, PB_CMD_SLC_SET_EOL);

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}

int pb_api_slc_revoke_key(struct pb_context *ctx, uint32_t key_id)
{
    int rc;
    struct pb_command cmd;
    struct pb_command_revoke_key revoke;
    struct pb_result result;

    ctx->d(ctx, 2, "%s: call\n", __func__);

    memset(&revoke, 0, sizeof(revoke));
    revoke.key_id = key_id;

    pb_wire_init_command2(&cmd, PB_CMD_SLC_REVOKE_KEY, &revoke, sizeof(revoke));

    rc = ctx->write(ctx, &cmd, sizeof(cmd));

    if (rc != PB_RESULT_OK)
        return rc;

    rc = ctx->read(ctx, &result, sizeof(result));

    if (rc != PB_RESULT_OK)
        return rc;

    if (!pb_wire_valid_result(&result))
        return -PB_RESULT_ERROR;

    ctx->d(ctx,
           2,
           "%s: return %i (%s)\n",
           __func__,
           result.result_code,
           pb_error_string(result.result_code));
    return result.result_code;
}
