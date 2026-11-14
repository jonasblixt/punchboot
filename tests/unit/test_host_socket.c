/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The host platform's transport (src/plat/host/socket.c, usb.md 2.6): a unix
 * socket with the six functions of every transport. 'init' listens and
 * returns at once, 'connect' is the accept and is polled like 'complete'
 * (usb.md 2.1, 2.2), so that command mode's ready timeout runs on the host as
 * on a board (command-mode.md 2.2). It blocked in the accept until a client
 * came (F-287): an alarm fails the test instead of letting it hang.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <limits.h>
#include <pb/cm.h>
#include <pb/dma.h>
#include <pb/pb.h>
#include <plat/host/host.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

/* socket.c's wait for the socket in a call, and the most a call may take */
#define POLL_MS     10
#define CALL_MAX_MS 1000
/* Polls of 'connect' or 'complete' before a client's bytes are given up on */
#define POLLS_MAX   (CALL_MAX_MS / POLL_MS)

struct host_options host_opts;

static char dir[] = "/tmp/pb-socket-XXXXXX";
static char path[PATH_MAX];

PB_DMA_ARRAY(uint8_t, buf, 512);

static void on_alarm(int sig)
{
    static const char msg[] = "A transport call blocked\n";

    (void)sig;
    (void)!write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(EXIT_FAILURE);
}

static unsigned int now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned int)(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

/* A host: connects to the socket, as the punchboot library does */
static int client_connect(void)
{
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);

    assert_true(fd >= 0);
    assert_true(strlen(path) < sizeof(addr.sun_path));
    memcpy(addr.sun_path, path, strlen(path) + 1);
    assert_int_equal(connect(fd, (struct sockaddr *)&addr, sizeof(addr)), 0);
    return fd;
}

/* Polls 'connect' until a client that connected is accepted */
static void connect_ok(void)
{
    int rc = -PB_ERR_AGAIN;

    for (int i = 0; i < POLLS_MAX && rc == -PB_ERR_AGAIN; i++)
        rc = cm_transport_connect();

    assert_int_equal(rc, PB_OK);
}

/* Polls 'complete' until the transfer ends, and returns its result */
static int complete(void)
{
    int rc = -PB_ERR_AGAIN;

    for (int i = 0; i < POLLS_MAX && rc == -PB_ERR_AGAIN; i++)
        rc = cm_transport_complete();

    return rc;
}

static int group_setup(void **state)
{
    (void)state;

    assert_non_null(mkdtemp(dir));
    snprintf(path, sizeof(path), "%s/pb.sock", dir);
    host_opts.socket_path = path;
    signal(SIGALRM, on_alarm);
    return 0;
}

static int group_teardown(void **state)
{
    (void)state;

    unlink(path);
    rmdir(dir);
    return 0;
}

/*
 * usb.md 2.1, 2.2, 2.6: 'init' doesn't wait for a host, and 'connect'
 * answers -PB_ERR_AGAIN at once, each call within its poll, until a client
 * connects. The path exists once the socket listens, which is what clients
 * wait for. A client that connected is accepted, and 'connect' stays
 * PB_OK while it is there.
 */
static void test_init_and_connect_dont_wait(void **state)
{
    unsigned int start;
    int fd;
    (void)state;

    alarm(5);

    start = now_ms();
    assert_int_equal(cm_transport_init(), PB_OK);
    assert_true(now_ms() - start < CALL_MAX_MS);
    assert_int_equal(access(path, F_OK), 0);

    for (int i = 0; i < 3; i++) {
        start = now_ms();
        assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
        assert_true(now_ms() - start < CALL_MAX_MS);
    }

    fd = client_connect();
    connect_ok();
    assert_int_equal(cm_transport_connect(), PB_OK);

    alarm(0);
    close(fd);
}

/*
 * usb.md 2.3, 2.4, 2.6: a session's transfers both ways, a client that
 * closes is lost, and a restart ('init' again) waits for the next client
 * without blocking: the next one is accepted by 'connect'.
 */
static void test_a_session_and_the_next_client(void **state)
{
    uint8_t host[sizeof(buf)];
    int fd;
    (void)state;

    alarm(5);

    assert_int_equal(cm_transport_init(), PB_OK);
    fd = client_connect();
    connect_ok();

    /* From the host */
    for (size_t i = 0; i < sizeof(host); i++)
        host[i] = (uint8_t)(i * 7 + 1);
    assert_int_equal(cm_transport_read(buf, sizeof(buf)), PB_OK);
    assert_int_equal(cm_transport_complete(), -PB_ERR_AGAIN);
    assert_int_equal(send(fd, host, sizeof(host), 0), (ssize_t)sizeof(host));
    assert_int_equal(complete(), PB_OK);
    assert_memory_equal(buf, host, sizeof(host));

    /* To the host */
    memset(host, 0, sizeof(host));
    for (size_t i = 0; i < sizeof(buf); i++)
        buf[i] = (uint8_t)(i * 3 + 2);
    assert_int_equal(cm_transport_write(buf, sizeof(buf)), PB_OK);
    assert_int_equal(complete(), PB_OK);
    assert_int_equal(recv(fd, host, sizeof(host), MSG_WAITALL), (ssize_t)sizeof(host));
    assert_memory_equal(buf, host, sizeof(host));

    /* The client closes: lost */
    close(fd);
    assert_int_equal(cm_transport_read(buf, sizeof(buf)), PB_OK);
    assert_int_equal(complete(), -PB_ERR_IO);

    /* The restart: no client yet, and nothing blocks */
    assert_int_equal(cm_transport_disconnect(), PB_OK);
    assert_int_equal(cm_transport_init(), PB_OK);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);

    /* The next one */
    fd = client_connect();
    connect_ok();
    host[0] = 0x5a;
    assert_int_equal(cm_transport_read(buf, 1), PB_OK);
    assert_int_equal(send(fd, host, 1, 0), 1);
    assert_int_equal(complete(), PB_OK);
    assert_int_equal(buf[0], 0x5a);

    alarm(0);
    close(fd);
}

/* A client that connects and closes before it is accepted is accepted and
 * then lost (Linux keeps it in the backlog), and leaves nothing for the
 * next: the next client's session is its own */
static void test_a_client_gone_before_the_accept(void **state)
{
    uint8_t byte = 0xa5;
    int fd;
    (void)state;

    alarm(5);

    assert_int_equal(cm_transport_init(), PB_OK);
    assert_int_equal(cm_transport_connect(), -PB_ERR_AGAIN);
    close(client_connect());

    connect_ok();
    assert_int_equal(cm_transport_read(buf, 1), PB_OK);
    assert_int_equal(complete(), -PB_ERR_IO);
    assert_int_equal(cm_transport_init(), PB_OK);

    fd = client_connect();
    connect_ok();
    assert_int_equal(cm_transport_read(buf, 1), PB_OK);
    assert_int_equal(send(fd, &byte, 1, 0), 1);
    assert_int_equal(complete(), PB_OK);
    assert_int_equal(buf[0], byte);

    alarm(0);
    close(fd);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_and_connect_dont_wait),
        cmocka_unit_test(test_a_session_and_the_next_client),
        cmocka_unit_test(test_a_client_gone_before_the_accept),
    };

    return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
