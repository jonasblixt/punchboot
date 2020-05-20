#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <pb-tools/usb.h>
#include <pb-tools/socket.h>
#include "tool.h"

int pb_debug(struct pb_context *ctx, int level, const char *fmt, ...)
{
    va_list args;

    if (pb_get_verbosity() >= level)
    {
        va_start(args, fmt);

        if (level == 0)
            vfprintf(stderr, fmt, args);
        else
            vprintf(fmt, args);
        va_end(args);
    }

    return PB_RESULT_OK;
}

int transport_init_helper(struct pb_context **ctxp, const char *transport_name,
                            const char *device_uuid)
{
    int rc;
    char *t = (char *) transport_name;

    if (!t)
        t = (char *) "usb";

    if (pb_get_verbosity() > 2)
        printf("Initializing transport: %s\n", t);

    struct pb_context *ctx = NULL;
    rc = pb_api_create_context(&ctx, pb_debug);

    if (rc != PB_RESULT_OK)
        return rc;
    *ctxp = ctx;

    if (strcmp(t, "usb") == 0)
    {
        rc = pb_usb_transport_init(ctx, device_uuid);
    }
    else if (strcmp(t, "socket") == 0)
    {
#ifdef WINDOWS
        printf("Error: The socket transport is not supported on windows\n");
        rc = -PB_RESULT_ERROR;
#else
        if (pb_get_verbosity() > 2)
            printf("Connecting to /tmp/pb.sock\n");
        rc = pb_socket_transport_init(ctx, "/tmp/pb.sock");
#endif
    }

    if (rc != PB_RESULT_OK)
        fprintf(stderr, "Error: Could not initialize transport\n");

    return rc;
}

int bytes_to_string(size_t bytes, char *out, size_t size)
{
    if (bytes > 1024*1024)
        snprintf(out, size, "%-4li MB", bytes / (1024*1024));
    else if (bytes > 1024)
        snprintf(out, size, "%-4li kB", bytes / 1024);
    else
        snprintf(out, size, "%-4li B ", bytes);

    return PB_RESULT_OK;
};
