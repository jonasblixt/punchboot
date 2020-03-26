#ifndef INCLUDE_PB_DEVICE_H_
#define INCLUDE_PB_DEVICE_H_

#include <stdint.h>
#include <stdbool.h>

struct pb_device;

typedef int (*pb_device_init_t) (struct pb_device *dev);

struct pb_device
{
    bool ready;
    pb_device_init_t platform_init;
    pb_device_init_t init;
    pb_device_init_t platform_free
    pb_device_init_t free;
    void *private;
};

#endif  // INCLUDE_PB_DEVICE_H_
