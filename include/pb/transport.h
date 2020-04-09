#ifndef INCLUDE_PB_TRANSPORT_H_
#define INCLUDE_PB_TRANSPORT_H_

#include <stdint.h>
#include <stdbool.h>
#include <pb-tools/wire.h>

struct pb_transport_driver;

typedef int (*pb_transport_call_t) (struct pb_transport_driver *drv);

typedef int (*pb_transport_io_t) (struct pb_transport_driver *drv,
                                    void *buf, size_t size);

struct pb_transport_plat_driver
{
    pb_transport_call_t init;
    pb_transport_call_t free;
    void *private;
    size_t size;
};

struct pb_transport_driver
{
    bool command_available;
    const char *name;
    bool ready;
    pb_transport_call_t init;
    pb_transport_call_t free;
    pb_transport_call_t process;
    pb_transport_io_t read;
    pb_transport_io_t write;
    struct pb_transport_plat_driver *platform;
    struct pb_command cmd;
    size_t max_chunk_bytes;
    uint8_t *device_uuid;
    void *private;
    size_t size;
};

struct pb_transport
{
    struct pb_transport_driver *driver;
    uint8_t *device_uuid;
};

int pb_transport_init(struct pb_transport *ctx, uint8_t *device_uuid);

int pb_transport_add(struct pb_transport *ctx,
                        struct pb_transport_driver *drv);

int pb_transport_free(struct pb_transport *ctx);
int pb_transport_start(struct pb_transport *ctx, int timeout_s);

int pb_transport_read(struct pb_transport *ctx,
                      void *buf,
                      size_t size);

int pb_transport_write(struct pb_transport *ctx,
                      void *buf,
                      size_t size);

#endif  // INCLUDE_PB_TRANSPORT_H_
