/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M SPL's fastboot on the USB stack (D-089, plat-imx.md 2) with a
 * host on the bus (fakes/usb_host.h): the device a host sees, a session of
 * commands, a download and the step that decides what starts, and what the
 * session does when the host stops or starts over. FB_USB_CHUNK is 1024
 * here: a download is read in pieces.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <drivers/usb/usbd.h>
#include <pb/pb.h>
#include <string.h>

#include "../../src/plat/imx8m/spl/fb_usb.h"
#include "fakes/plat.h"
#include "fakes/usb_host.h"

#define ROOM   (8 * USB_HOST_PACKET)
#define CANARY 0x5a
#define SECOND 1000000u

/* Where a download goes, and what is behind its room */
static uint8_t dest[ROOM + 64] __attribute__((aligned(64)));
static uint8_t image[ROOM];

static int accept_rc;
static int accept_calls;
static size_t accept_size;

static int accept(size_t downloaded)
{
    accept_calls++;
    accept_size = downloaded;
    return accept_rc;
}

static const struct fb_usb_config config = {
    .dest = dest,
    .room = ROOM,
    .accept = accept,
};

static void start(const struct usb_host_config *cfg)
{
    usb_host_reset(cfg);
    fake_us_tick = FAKE_US_TICK_DEFAULT;
    fake_us_tick_step = 0;
    accept_rc = PB_OK;
    accept_calls = 0;
    accept_size = 0;
    memset(dest, CANARY, sizeof(dest));

    for (size_t i = 0; i < sizeof(image); i++)
        image[i] = (uint8_t)(i * 13 + (i >> 8));

    assert_int_equal(usbd_init_hal_ops(usb_host_hal_ops()), PB_OK);
    assert_int_equal(fb_usb_start(&config), PB_OK);
    assert_true(usb_host_state()->attached);
}

static void host_get_descriptor(uint8_t type, uint8_t index, uint16_t length)
{
    usb_host_request(
        USB_HOST_REQ_IN, USB_HOST_GET_DESCRIPTOR, USB_DESC_VALUE(type, index), 0, length);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_true(usb_host_reply()->status);
}

/* What a host does with a new device, as Linux */
static void enumerate(void)
{
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    usb_host_request(0, USB_HOST_SET_ADDRESS, 5, 0, 0);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 32);
    usb_host_request(0, USB_HOST_SET_CONFIGURATION, 1, 0, 0);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_true(usb_host_state()->configured);
}

/* The host reads the answer that waits for it */
static const char *host_answer(void)
{
    static char answer[FB_USB_PACKET + 1];
    size_t n = usb_host_receive(answer, FB_USB_PACKET);

    answer[n] = '\0';
    return answer;
}

/* The host sends a command, the device answers and the host reads it. The
 * session goes on */
static const char *host_command(const char *text)
{
    const char *answer;

    usb_host_send(text, strlen(text));
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    answer = host_answer();
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    return answer;
}

static void assert_room_kept(void)
{
    for (size_t i = ROOM; i < sizeof(dest); i++)
        assert_int_equal(dest[i], CANARY);
}

/* A download of 'size' bytes that the host sends as it announced */
static void host_download(size_t size)
{
    char cmd[32];

    (void)snprintf(cmd, sizeof(cmd), "download:%08x", (unsigned int)size);
    assert_memory_equal(host_command(cmd), "DATA", 4);
    usb_host_send(image, size);

    /* A round per piece, and the answer after the last */
    for (size_t done = 0; done < size; done += 1024)
        assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);

    assert_string_equal(host_answer(), "OKAY");
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_memory_equal(dest, image, size);
    assert_room_kept();
}

/* D-089, usb.md 6: pb's vendor id, a product id of the SPL's own, the
 * interface fastboot's hosts look for, and nothing that names the device */
static void test_device(void **state)
{
    const struct usb_host_reply *r = usb_host_reply();
    (void)state;

    start(NULL);
    host_get_descriptor(USB_DESC_TYPE_DEVICE, 0, 64);
    assert_int_equal(r->length, 18);
    assert_int_equal(r->data[8] | (r->data[9] << 8), 0x1209);
    assert_int_equal(r->data[10] | (r->data[11] << 8), 0x0001);
    assert_int_not_equal(r->data[10] | (r->data[11] << 8), 0x2019);
    assert_int_equal(r->data[16], 0); /* No serial number */

    host_get_descriptor(USB_DESC_TYPE_CONFIG, 0, 255);
    assert_int_equal(r->length, 32);
    assert_int_equal(r->data[9 + 5], 0xff);
    assert_int_equal(r->data[9 + 6], 0x42);
    assert_int_equal(r->data[9 + 7], 0x03);
    assert_int_equal(r->data[18 + 2], 0x81);
    assert_int_equal(r->data[25 + 2], 0x02);

    host_get_descriptor(USB_DESC_TYPE_STRING, 1, 255);
    assert_int_equal(r->length, 2 + 2 * strlen("Punchboot SPL"));
    assert_int_equal(r->data[2], 'P');
    assert_int_equal(r->data[r->length - 2], 'L');
}

static void test_getvar(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    assert_string_equal(host_command("getvar:version"), "OKAY0.4");
    assert_string_equal(host_command("getvar:max-download-size"), "OKAY0x00001000");
    assert_string_equal(host_command("getvar:serialno"), "FAILunknown variable");
    assert_string_equal(host_command("flash:boot"), "FAILunknown command");
    assert_int_equal(accept_calls, 0);
}

/* The whole session: a download in pieces, the step that decides says yes,
 * the host gets its answer and the device leaves the bus */
static void test_download_boot(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    host_download(2500);
    assert_int_equal(accept_calls, 0);

    usb_host_send("boot", 4);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_int_equal(accept_calls, 1);
    assert_int_equal(accept_size, 2500);

    /* Not before the host has its answer */
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_true(usb_host_state()->attached);
    assert_string_equal(host_answer(), "OKAY");
    assert_int_equal(fb_usb_poll(), PB_OK);
    assert_false(usb_host_state()->attached);
    assert_int_equal(usb_host_state()->stop_calls, 1);

    /* The session is over */
    assert_int_equal(fb_usb_poll(), -PB_ERR_STATE);
    assert_int_equal(accept_calls, 1);
}

/* The room's last byte, and uuu's and fastboot's other word for boot */
static void test_download_room(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    assert_string_equal(host_command("download:00001001"), "FAILtoo large");
    host_download(ROOM);

    usb_host_send("continue", 8);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_int_equal(accept_size, ROOM);
    assert_string_equal(host_answer(), "OKAY");
    assert_int_equal(fb_usb_poll(), PB_OK);
}

/* What the step refuses doesn't start, and isn't tried again */
static void test_refused(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    host_download(1000);

    accept_rc = -PB_ERR_SIGNATURE;
    assert_string_equal(host_command("boot"), "FAILnot accepted");
    assert_int_equal(accept_calls, 1);
    assert_true(usb_host_state()->attached);

    accept_rc = PB_OK;
    assert_string_equal(host_command("boot"), "FAILnothing downloaded");
    assert_int_equal(accept_calls, 1);

    /* Another download is */
    host_download(600);
    usb_host_send("boot", 4);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_int_equal(accept_calls, 2);
    assert_int_equal(accept_size, 600);
}

/* A host that sends less than it announced: nothing to boot */
static void test_download_short(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    assert_string_equal(host_command("download:000007d0"), "DATA000007d0");
    usb_host_send(image, 1000);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_string_equal(host_answer(), "FAILdownload failed");
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);

    assert_string_equal(host_command("boot"), "FAILnothing downloaded");
    assert_int_equal(accept_calls, 0);
    assert_room_kept();
}

/* A host that sends more than it announced: the download is what was
 * announced, and what follows is no command */
static void test_download_more(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    assert_string_equal(host_command("download:00000400"), "DATA00000400");
    usb_host_send(image, 1024 + 300);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_string_equal(host_answer(), "OKAY");
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_int_equal(dest[1024], CANARY);

    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_string_equal(host_answer(), "FAILcommand too long");
    assert_room_kept();
}

/* A command is at most FB_CMD_MAX bytes: a longer one, a whole packet too,
 * is refused and the session goes on */
static void test_long_command(void **state)
{
    char cmd[USB_HOST_PACKET + 1];
    (void)state;

    start(NULL);
    enumerate();

    memset(cmd, 'a', sizeof(cmd));
    cmd[65] = '\0';
    assert_string_equal(host_command(cmd), "FAILcommand too long");

    cmd[65] = 'a';
    cmd[USB_HOST_PACKET] = '\0';
    assert_string_equal(host_command(cmd), "FAILcommand too long");
    assert_string_equal(host_command("getvar:product"), "OKAYpunchboot-spl");
}

/* No host configures the device in its time: the caller resets */
static void test_no_host(void **state)
{
    (void)state;

    start(NULL);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    fake_us_tick += 10 * SECOND - 1;
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    fake_us_tick += 1;
    assert_int_equal(fb_usb_poll(), -PB_ERR_TIMEOUT);
    assert_false(usb_host_state()->attached);
}

/* No time runs while the device waits for a command */
static void test_command_wait(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    fake_us_tick += 600 * SECOND;
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_string_equal(host_command("getvar:version"), "OKAY0.4");
    assert_int_equal(usb_host_state()->init_calls, 1);
}

static void assert_restarted(void)
{
    assert_int_equal(usb_host_state()->stop_calls, 1);
    assert_int_equal(usb_host_state()->init_calls, 2);
    assert_true(usb_host_state()->attached);
    enumerate();
    assert_string_equal(host_command("boot"), "FAILnothing downloaded");
    assert_int_equal(accept_calls, 0);
}

/* A bus reset in a session: lost (usb.md 2.4). The device starts over, and
 * what was downloaded is gone */
static void test_lost(void **state)
{
    static const struct usb_host_config cfg = { .reports_reset = true };
    (void)state;

    start(&cfg);
    enumerate();
    host_download(1000);
    usb_host_bus_reset();
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_restarted();
}

/* A host that enumerates the device again inside a session */
static void test_enumerated_again(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    host_download(1000);
    usb_host_request(0, USB_HOST_SET_ADDRESS, 7, 0, 0);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_restarted();
}

/* A host that announces a download and sends none of it */
static void test_data_timeout(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    host_download(1000);
    assert_string_equal(host_command("download:00000400"), "DATA00000400");
    fake_us_tick += 10 * SECOND - 1;
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_int_equal(usb_host_state()->init_calls, 1);
    fake_us_tick += 1;
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_int_equal(usb_host_state()->cancel_calls, 1);
    assert_restarted();
}

/* A host that doesn't read an answer */
static void test_reply_timeout(void **state)
{
    (void)state;

    start(NULL);
    enumerate();
    usb_host_send("getvar:version", 14);
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    fake_us_tick += 10 * SECOND;
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    assert_restarted();
}

/* The no host's time runs again after a restart */
static void test_restart_no_host(void **state)
{
    static const struct usb_host_config cfg = { .reports_reset = true };
    (void)state;

    start(&cfg);
    enumerate();
    fake_us_tick += 60 * SECOND;
    usb_host_bus_reset();
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    fake_us_tick += 10 * SECOND - 1;
    assert_int_equal(fb_usb_poll(), -PB_ERR_AGAIN);
    fake_us_tick += 1;
    assert_int_equal(fb_usb_poll(), -PB_ERR_TIMEOUT);
}

/* The controller fails */
static void test_controller_fails(void **state)
{
    (void)state;

    start(NULL);
    usb_host_fail_next_poll(-PB_ERR_IO);
    assert_int_equal(fb_usb_poll(), -PB_ERR_IO);
}

/* A place a download can't go to, and no step that decides */
static void test_config(void **state)
{
    struct fb_usb_config cfg = config;
    (void)state;

    usb_host_reset(NULL);
    assert_int_equal(usbd_init_hal_ops(usb_host_hal_ops()), PB_OK);

    assert_int_equal(fb_usb_start(NULL), -PB_ERR_PARAM);
    cfg.accept = NULL;
    assert_int_equal(fb_usb_start(&cfg), -PB_ERR_PARAM);
    cfg = config;
    cfg.dest = dest + 8;
    assert_int_equal(fb_usb_start(&cfg), -PB_ERR_PARAM);
    cfg = config;
    cfg.room = ROOM - 1;
    assert_int_equal(fb_usb_start(&cfg), -PB_ERR_PARAM);
    cfg.room = 0;
    assert_int_equal(fb_usb_start(&cfg), -PB_ERR_PARAM);
    assert_int_equal(usb_host_state()->init_calls, 0);
    assert_int_equal(fb_usb_poll(), -PB_ERR_STATE);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_device),
        cmocka_unit_test(test_getvar),
        cmocka_unit_test(test_download_boot),
        cmocka_unit_test(test_download_room),
        cmocka_unit_test(test_refused),
        cmocka_unit_test(test_download_short),
        cmocka_unit_test(test_download_more),
        cmocka_unit_test(test_long_command),
        cmocka_unit_test(test_no_host),
        cmocka_unit_test(test_command_wait),
        cmocka_unit_test(test_lost),
        cmocka_unit_test(test_enumerated_again),
        cmocka_unit_test(test_data_timeout),
        cmocka_unit_test(test_reply_timeout),
        cmocka_unit_test(test_restart_no_host),
        cmocka_unit_test(test_controller_fails),
        cmocka_unit_test(test_config),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
