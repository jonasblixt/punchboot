#ifndef INCLUDE_PB_COMMAND_H_
#define INCLUDE_PB_COMMAND_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb/protocol.h>

struct pb_command_ctx;

typedef int (*pb_command_device_reset_t) (struct pb_command_ctx *ctx);
typedef int (*pb_command_send_result_t) (struct pb_command_ctx *ctx,
                                         struct pb_result *result);

typedef int (*pb_command_device_read_uuid_t) (struct pb_command_ctx *ctx,
                                         struct pb_result *result);

typedef int (*pb_command_read_slc_t) (struct pb_command_ctx *ctx,
                                         struct pb_result *result);

typedef int (*pb_command_authenticate_t) (struct pb_command_ctx *ctx,
                                         struct pb_command *command);

typedef int (*pb_command_config_t) (struct pb_command_ctx *ctx,
                                    struct pb_command *command);

typedef int (*pb_command_config_lock_t) (struct pb_command_ctx *ctx,
                                    struct pb_command *command);

struct pb_command_ctx
{
    bool authenticated;
    pb_command_device_reset_t device_reset;
    pb_command_send_result_t send_result;
    pb_command_device_read_uuid_t device_read_uuid;
    pb_command_read_slc_t read_slc;
    pb_command_authenticate_t authenticate;
    pb_command_config_t device_config;
    pb_command_config_lock_t device_config_lock;
};

int pb_command_init(struct pb_command_ctx *ctx);
int pb_command_free(struct pb_command_ctx *ctx);
int pb_command_process(struct pb_command_ctx *ctx, struct pb_command *command);

#endif  // INCLUDE_PB_COMMAND_H_
