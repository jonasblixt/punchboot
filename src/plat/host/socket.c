/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode transport over a unix domain socket: the host platform's
 * implementation of the cm_transport_*() functions (pb/cm.h, usb.md 2.6),
 * which pb-host links in place of the USB class driver.
 *
 * The command mode loop uses an asynchronous API: 'read'/'write' queue a
 * transfer and 'complete' polls it (usb.md 2.4): it moves what the socket
 * has, waits at most POLL_MS for more, and returns -PB_ERR_AGAIN until the
 * transfer is whole, so that command mode's transfer timeout and watchdog
 * kicks run (command-mode.md 4.6). When the client disconnects, 'complete'
 * fails, which makes the command mode loop call 'init' again, and that
 * waits for the next client.
 */

#include <errno.h>
#include <pb/cm.h>
#include <pb/pb.h>
#include <plat/host/host.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* The longest a call to 'complete' waits for the socket */
#define POLL_MS 10

static int listen_fd = -1;
static int client_fd = -1;

static struct {
    bool pending;
    bool is_read;
    void *buf;
    size_t length;
    size_t done;
} xfer;

/* Listens on a temporary path and then renames it to the socket path, so
 * the socket path only exists once it accepts connections. Clients wait for
 * the path to appear. */
static int socket_listen(void)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };

    if (strlen(host_opts.socket_path) + strlen(".tmp") >= sizeof(addr.sun_path)) {
        LOG_ERR("Socket path too long");
        return -PB_ERR_PARAM;
    }

    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s.tmp", host_opts.socket_path);
    unlink(addr.sun_path);

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

    if (rename(addr.sun_path, host_opts.socket_path) != 0) {
        LOG_ERR("Could not rename '%s' (%s)", addr.sun_path, strerror(errno));
        close(listen_fd);
        listen_fd = -1;
        return -PB_ERR_IO;
    }

    return PB_OK;
}

int cm_transport_init(void)
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

/* The accept is in init, which blocks until a client connects (F-287):
 * nothing is left to wait for */
int cm_transport_connect(void)
{
    return PB_OK;
}

/* Nothing, as before the transport was linked: the connection stays until
 * init accepts the next client or the process exits */
int cm_transport_disconnect(void)
{
    return PB_OK;
}

int cm_transport_read(void *buf, size_t length)
{
    xfer.pending = true;
    xfer.is_read = true;
    xfer.buf = buf;
    xfer.length = length;
    xfer.done = 0;
    return PB_OK;
}

int cm_transport_write(const void *buf, size_t length)
{
    xfer.pending = true;
    xfer.is_read = false;
    xfer.buf = (void *)buf;
    xfer.length = length;
    xfer.done = 0;
    return PB_OK;
}

int cm_transport_complete(void)
{
    struct pollfd pfd = { .fd = client_fd };

    if (!xfer.pending)
        return PB_OK;

    pfd.events = xfer.is_read ? POLLIN : POLLOUT;

    while (xfer.done < xfer.length) {
        uint8_t *p = (uint8_t *)xfer.buf + xfer.done;
        size_t remaining = xfer.length - xfer.done;
        ssize_t bytes;
        int ready = poll(&pfd, 1, POLL_MS);

        if (ready == 0 || (ready < 0 && errno == EINTR))
            return -PB_ERR_AGAIN;

        if (ready < 0) {
            xfer.pending = false;
            return -PB_ERR_IO;
        }

        if (xfer.is_read)
            bytes = recv(client_fd, p, remaining, MSG_DONTWAIT);
        else
            bytes = send(client_fd, p, remaining, MSG_DONTWAIT);

        if (bytes < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
            continue;

        if (bytes <= 0) {
            /* Error, or the client disconnected */
            xfer.pending = false;
            return -PB_ERR_IO;
        }

        xfer.done += (size_t)bytes;
    }

    xfer.pending = false;
    return PB_OK;
}
