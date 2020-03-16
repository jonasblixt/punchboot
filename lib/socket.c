
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <pb/api.h>
#include <pb/protocol.h>
#include <pb/error.h>
#include <pb/socket.h>

struct pb_socket_private
{
    int fd;
    struct sockaddr_un addr;
    const char *socket_path;
};

#define PB_SOCKET_PRIVATE(__ctx) ((struct pb_socket_private *) ctx->transport)


static int pb_socket_connect(struct pb_context *ctx)
{
    int rc;
    struct pb_socket_private *priv = PB_SOCKET_PRIVATE(ctx);


    rc = connect(priv->fd, (struct sockaddr*) &priv->addr,
                            sizeof(priv->addr));

    if (rc != 0)
        return -PB_ERR;

    return PB_OK;
}

static int pb_socket_command(struct pb_context *ctx,
                            struct pb_command *cmd,
                            const void *bfr, size_t sz)
{
    int rc;
    struct pb_socket_private *priv = PB_SOCKET_PRIVATE(ctx);


    ssize_t bytes = write(priv->fd, cmd, sizeof(*cmd));

    if (bytes != sizeof(*cmd))
        return -PB_ERR;

    if (sz)
    {
        bytes = write(priv->fd, bfr, sz);

        if (bytes != sz)
            return -PB_ERR;
    }

    return PB_OK;
}


static int pb_socket_read(struct pb_context *ctx, void *bfr, size_t sz)
{
    struct pb_socket_private *priv = PB_SOCKET_PRIVATE(ctx);

    ssize_t bytes = read(priv->fd, bfr, sz);

    if (bytes == sz)
        return PB_OK;

    return -PB_ERR;
}

static int pb_socket_init(struct pb_context *ctx)
{
    int rc;
    struct pb_socket_private *priv = PB_SOCKET_PRIVATE(ctx);

    priv->fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (priv->fd == -1)
    {
        rc = -PB_ERR;
        goto err_free_transport;
    }

    priv->addr.sun_family = AF_UNIX;

    strncpy(priv->addr.sun_path, priv->socket_path,
                sizeof(priv->addr.sun_path)-1);
    
    return PB_OK;

err_free_transport:
    free(ctx->transport);
    return rc;
}

static int pb_socket_free(struct pb_context *ctx)
{
    struct pb_socket_private *priv = PB_SOCKET_PRIVATE(ctx);

    /* Closing the socket too quickley makes qemu behave bad.
        Behaviour is that qemu will hang while sending data even though all
        data has been received. */

    usleep(20000);
    close(priv->fd);
    free(ctx->transport);
    return PB_OK;
}

int pb_socket_transport_init(struct pb_context *ctx,
                             const char socket_path[])
{
    ctx->transport = malloc(sizeof(struct pb_socket_private));

    if (!ctx->transport)
        return -PB_NO_MEMORY;

    memset(ctx->transport, 0, sizeof(struct pb_socket_private));

    struct pb_socket_private *priv = PB_SOCKET_PRIVATE(ctx);

    ctx->free = pb_socket_free;
    ctx->init = pb_socket_init;
    ctx->read = pb_socket_read;
    ctx->command = pb_socket_command;
    ctx->connect = pb_socket_connect;
    priv->socket_path = socket_path;

    return ctx->init(ctx);
}

