#ifndef INCLUDE_PB_API_H_
#define INCLUDE_PB_API_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb/protocol.h>

struct pb_context;

typedef int (*pb_init_t) (struct pb_context *ctx);
typedef int (*pb_free_t) (struct pb_context *ctx);

typedef int (*pb_command_t) (struct pb_context *ctx,
                               uint32_t command,
                               uint32_t arg0,
                               uint32_t arg1,
                               uint32_t arg2,
                               uint32_t arg3,
                               const void *bfr,
                               size_t sz);

typedef int (*pb_write_t) (struct pb_context *ctx, const void *bfr,
                                    size_t sz);

typedef int (*pb_read_t) (struct pb_context *ctx, void *bfr, size_t sz);

typedef int (*pb_list_devices_t) (struct pb_context *ctx);

typedef int (*pb_connect_t) (struct pb_context *ctx);
typedef int (*pb_disconnect_t) (struct pb_context *ctx);

struct pb_context
{
    bool connected;
    pb_init_t init;
    pb_free_t free;
    pb_command_t command;
    pb_write_t write;
    pb_read_t read;
    pb_list_devices_t list;
    pb_connect_t connect;
    pb_disconnect_t disconnect;
    void *transport;
};

int pb_create_context(struct pb_context **ctx);
int pb_free_context(struct pb_context *ctx);

int pb_get_version(struct pb_context *ctx, char *output, size_t sz);
int pb_device_reset(struct pb_context *ctx);

#endif  // INCLUDE_PB_API_H_
