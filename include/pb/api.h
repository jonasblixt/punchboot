#ifndef INCLUDE_PB_API_H_
#define INCLUDE_PB_API_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb/protocol.h>

struct pb_context;

typedef int (*pb_init_t) (struct pb_context *ctx);
typedef int (*pb_free_t) (struct pb_context *ctx);

typedef int (*pb_command_t) (struct pb_context *ctx,
                               struct pb_command *cmd,
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
    struct pb_command last_command;
    struct pb_result last_result;
};

int pb_create_context(struct pb_context **ctx);
int pb_free_context(struct pb_context *ctx);

int pb_get_version(struct pb_context *ctx, char *version, size_t size);
int pb_device_reset(struct pb_context *ctx);
int pb_device_read_uuid(struct pb_context *ctx,
                            struct pb_result_device_uuid *out);

int pb_api_authenticate(struct pb_context *ctx,
                        enum pb_auth_method method,
                        uint8_t *data,
                        size_t size);

int pb_api_device_read_slc(struct pb_context *ctx, uint32_t *slc);

int pb_api_device_configure(struct pb_context *ctx, const void *data,
                                size_t size);

int pb_api_device_configuration_lock(struct pb_context *ctx, const void *data,
                                size_t size);

#endif  // INCLUDE_PB_API_H_
