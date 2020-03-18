#ifndef INCLUDE_PB_API_H_
#define INCLUDE_PB_API_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb-tools/api.h>

struct pb_context;
struct pb_command;

typedef int (*pb_init_t) (struct pb_context *ctx);
typedef int (*pb_free_t) (struct pb_context *ctx);

typedef int (*pb_api_command_t) (struct pb_context *ctx,
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
    pb_api_command_t command;
    pb_write_t write;
    pb_read_t read;
    pb_list_devices_t list;
    pb_connect_t connect;
    pb_disconnect_t disconnect;
    void *transport;
};

int pb_api_create_context(struct pb_context **ctx);
int pb_api_free_context(struct pb_context *ctx);

int pb_api_read_bootloader_version(struct pb_context *ctx, char *version,
                                    size_t size);

int pb_api_device_reset(struct pb_context *ctx);

int pb_api_device_read_identifier(struct pb_context *ctx,
                                  uint8_t *device_uuid,
                                  size_t device_uuid_size,
                                  char *board_id,
                                  size_t board_id_size);

int pb_api_authenticate_password(struct pb_context *ctx,
                                    uint8_t *data,
                                    size_t size);

int pb_api_authenticate_key(struct pb_context *ctx,
                            uint32_t key_id,
                            uint8_t *data,
                            size_t size);

int pb_api_device_read_slc(struct pb_context *ctx, uint32_t *slc);

int pb_api_device_configure(struct pb_context *ctx, const void *data,
                                size_t size);

int pb_api_device_configuration_lock(struct pb_context *ctx, const void *data,
                                size_t size);
/*
int pb_api_partition_read(struct pb_context *ctx,
                          struct pb_partition_table_entry *out,
                          size_t size);
*/
#endif  // INCLUDE_PB_API_H_
