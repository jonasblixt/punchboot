/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_CM_H
#define INCLUDE_CM_H

#include <config.h>
#include <stddef.h>
#include <stdint.h>

/* With the gate a closed device opens only to a method: a build without one
 * answers NOT_SUPPORTED to AUTHENTICATE for good once locked
 * (command-mode.md 3.2, D-059) */
#if defined(CONFIG_CM_AUTH) && !defined(CONFIG_CM_AUTH_TOKEN) && !defined(CONFIG_CM_AUTH_PASSWORD)
#error "CONFIG_CM_AUTH needs a method: CONFIG_CM_AUTH_TOKEN or CONFIG_CM_AUTH_PASSWORD"
#endif

struct cm_config {
    const char *name;
    int (*status)(uint8_t *buf, size_t *length);
    int (*password_auth)(const char *password, size_t length);
    int (*command)(uint32_t cmd, uint8_t *bfr, size_t bfr_length, uint8_t *rsp, size_t *rsp_size);
    void (*process)(void);
};

/*
 * The transport (doc/interfaces/usb.md 2): what carries command mode's
 * bytes. Command mode calls these six functions and nothing else of the
 * layers below. The build links one implementation: the USB class driver
 * (src/drivers/usb/pb_dev_cls.c), or the host platform's socket for the
 * tests (src/plat/host/socket.c). Each returns PB_OK or -PB_ERR_*.
 */

/**
 * Start the transport: on USB the class, the stack and the controller, which
 * attaches to the bus. After plat_cm_init(), and again each time the
 * transport is restarted. It doesn't wait for a host.
 */
int cm_transport_init(void);

/**
 * Polled until a host is there: PB_OK when the transport is ready (USB: the
 * host configured the device), -PB_ERR_AGAIN while it isn't.
 */
int cm_transport_connect(void);

/**
 * Start a transfer of 'length' bytes from the host into 'buf', or from 'buf'
 * to the host. One at a time; cm_transport_complete() says when it is done,
 * and the buffer is the transport's until then.
 */
int cm_transport_read(void *buf, size_t length);
int cm_transport_write(const void *buf, size_t length);

/**
 * Poll the transfer that was started: PB_OK when it is done, -PB_ERR_AGAIN
 * while it isn't. Any other error: the host is gone or started over.
 */
int cm_transport_complete(void);

/**
 * The device leaves: on USB the controller is stopped. Before a jump and
 * before a reset.
 */
int cm_transport_disconnect(void);

/**
 * Start command mode
 *
 * This function calls plat_cm_init(), which sets up the USB device port, and
 * 'cm_board_init' (src/board.c), which checks the board's data and calls
 * 'cm_init'
 *
 * @return This function does not return normally
 */
int cm_run(void);

/**
 * Set up command mode for the board: the board's data that command mode uses
 * is checked, then cm_init() with the board's command mode config
 * (src/board.c)
 *
 * @return PB_OK, on success
 */
int cm_board_init(void);

/**
 * Request a reboot from a board command.
 *
 * This function can be called by a board command to request a system reboot
 * after the result of the command has been send to the host.
 *
 * The reboot is only requested if the board command returns zero (PB_OK).
 */
void cm_board_cmd_request_reboot_on_success(void);

/**
 * Populate cm configuration struct
 *
 * @return PB_OK, on success
 */
int cm_init(const struct cm_config *cfg);

#endif // INCLUDE_CM_H
