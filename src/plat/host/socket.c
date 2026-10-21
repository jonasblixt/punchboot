/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode transport over a unix domain socket.
 *
 * The command mode loop uses an asynchronous API: 'read'/'write' queue a
 * transfer and 'complete' finishes it. Here the transfer is performed
 * synchronously in 'complete'. When the client disconnects, 'complete'
 * fails, which makes the command mode loop call 'init' again, and that
 * waits for the next client.
 */

#include <errno.h>
#include <pb/pb.h>
#include <plat/host/host.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int listen_fd = -1;
static int client_fd = -1;

static struct {
    bool pending;
    bool is_read;
    void *buf;
    size_t length;
} xfer;

static int socket_listen(void)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };

    if (strlen(host_opts.socket_path) >= sizeof(addr.sun_path)) {
        LOG_ERR("Socket path too long");
        return -PB_ERR_PARAM;
    }

    strcpy(addr.sun_path, host_opts.socket_path);
    unlink(host_opts.socket_path);

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (listen_fd < 0) {
        LOG_ERR("socket: %s", strerror(errno));
        return -PB_ERR_IO;
    }

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(listen_fd, 1) != 0) {
        LOG_ERR("Could not listen on '%s' (%s)", host_opts.socket_path, strerror(errno));
        close(listen_fd);
        listen_fd = -1;
        return -PB_ERR_IO;
    }

    return PB_OK;
}

int host_socket_init(void)
{
    int rc;

    if (client_fd >= 0) {
        close(client_fd);
        client_fd = -1;
    }

    if (listen_fd < 0) {
        rc = socket_listen();

        if (rc != PB_OK)
            return rc;
    }

    LOG_DBG("Waiting for connection on '%s'", host_opts.socket_path);

    client_fd = accept(listen_fd, NULL, NULL);

    if (client_fd < 0) {
        LOG_ERR("accept: %s", strerror(errno));
        return -PB_ERR_IO;
    }

    return PB_OK;
}

int host_socket_read(void *buf, size_t length)
{
    xfer.pending = true;
    xfer.is_read = true;
    xfer.buf = buf;
    xfer.length = length;
    return PB_OK;
}

int host_socket_write(const void *buf, size_t length)
{
    xfer.pending = true;
    xfer.is_read = false;
    xfer.buf = (void *)buf;
    xfer.length = length;
    return PB_OK;
}

int host_socket_complete(void)
{
    uint8_t *p = xfer.buf;
    size_t remaining = xfer.length;

    if (!xfer.pending)
        return PB_OK;

    xfer.pending = false;

    while (remaining > 0) {
        ssize_t bytes;

        if (xfer.is_read)
            bytes = read(client_fd, p, remaining);
        else
            bytes = write(client_fd, p, remaining);

        if (bytes < 0 && errno == EINTR)
            continue;

        if (bytes <= 0) {
            /* Error, or the client disconnected */
            return -PB_ERR_IO;
        }

        p += bytes;
        remaining -= (size_t)bytes;
    }

    return PB_OK;
}
