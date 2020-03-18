#ifndef INCLUDE_PB_COMMAND_H_
#define INCLUDE_PB_COMMAND_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb-tools/wire.h>

#define PB_MAX_COMMANDS 32

struct pb_command_ctx;


typedef int (*pb_command_send_result_t) (const struct pb_command_ctx *ctx,
                                         struct pb_result *result);

typedef int (*pb_command_io_t) (const struct pb_command_ctx *ctx,
                                  void *buf, size_t size);

typedef int (*pb_command_t) (struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result);
struct pb_command_ctx
{
    bool authenticated;
    pb_command_io_t read;
    pb_command_io_t write;
    pb_command_send_result_t send_result;
    pb_command_t commands[PB_MAX_COMMANDS];
};

int pb_command_init(struct pb_command_ctx *ctx,
                    pb_command_send_result_t send_result,
                    pb_command_io_t read,
                    pb_command_io_t write);

int pb_command_free(struct pb_command_ctx *ctx);
int pb_command_process(struct pb_command_ctx *ctx, struct pb_command *command);

int pb_command_configure(struct pb_command_ctx *ctx,
                         enum pb_commands command_index,
                         pb_command_t command);


#endif  // INCLUDE_PB_COMMAND_H_
