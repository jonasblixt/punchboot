/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Command mode (command-mode.md): the session, its transfers, the gate, the
 * dispatch of a command, and the task. The handlers are in the other files
 * of src/cm/ (cm_internal.h).
 */

#include "cm_internal.h"
#include <boot/linux_ramoops.h>
#include <pb/command.h>
#include <pb/delay.h>
#include <pb/device_uuid.h>
#include <pb/plat.h>
#include <pb/task.h>
#include <string.h>

struct pb_command cm_cmd __section(".no_init") __aligned(64);
struct pb_result cm_result __section(".no_init") __aligned(64);
bool cm_authenticated = false;
uint8_t cm_buffer[2][CM_BUF_SIZE] __section(".no_init") __aligned(4096);

/* What the handlers put in the buffers without a check at run time fits a
 * build's buffers: the 1024 bytes of AUTHENTICATE and of a board's request
 * in one, a partition table entry per block device in the two, whose count
 * is a byte on the wire */
_Static_assert(CM_BUF_SIZE >= CM_DATA_XFER_SIZE, "CONFIG_CM_BUF_SIZE_KiB: below a data transfer");
_Static_assert(CONFIG_BIO_MAX_DEVS <= UINT8_MAX,
               "CONFIG_BIO_MAX_DEVS: the table's count is a byte");
_Static_assert(CONFIG_BIO_MAX_DEVS * sizeof(struct pb_result_part_table_entry) <= sizeof(cm_buffer),
               "CONFIG_CM_BUF_SIZE_KiB: no room for CONFIG_BIO_MAX_DEVS table entries");
slc_t cm_slc;

/* The part table response sends the low byte of the bio flags and the SLC
 * response sends slc_t, so both must match the wire values (D-008). */
_Static_assert(BIO_FLAG_BOOTABLE == PB_WIRE_PART_FLAG_BOOTABLE, "bio flag");
_Static_assert(BIO_FLAG_WRITABLE == PB_WIRE_PART_FLAG_WRITABLE, "bio flag");
_Static_assert(BIO_FLAG_ERASE_BEFORE_WRITE == PB_WIRE_PART_FLAG_ERASE_BEFORE_WRITE, "bio flag");
_Static_assert((int)SLC_NOT_CONFIGURED == (int)PB_SLC_NOT_CONFIGURED, "slc value");
_Static_assert((int)SLC_CONFIGURATION == (int)PB_SLC_CONFIGURATION, "slc value");
_Static_assert((int)SLC_CONFIGURATION_LOCKED == (int)PB_SLC_CONFIGURATION_LOCKED, "slc value");
_Static_assert((int)SLC_EOL == (int)PB_SLC_EOL, "slc value");
uint8_t cm_hash[CRYPTO_MD_MAX_SZ];
uuid_t cm_device_uu;

static bool reboot_requested = false;
const struct cm_config *cm_cfg;
/* A transfer inside a command failed or timed out (command-mode.md 4.4,
 * 4.6): the session is over, the command's other transfers fail at once and
 * the task restarts the transport */
static bool transfer_lost;
/* A command ended command mode (command-mode.md 2.3): DEVICE_RESET, a
 * named command that asked for the reset and returned PB_OK (5.6), a boot
 * that returned after the disconnect. The task leaves when the command is
 * done. Nothing else leaves: an error a command returns, -PB_ERR_ABORT too,
 * is the host's result, and command mode goes on (board.md 5.4) */
bool cm_leave_requested;
/* The device left the bus, until the transport is started again: it leaves
 * once, before a jump or before the reset */
static bool disconnected;

#ifdef CONFIG_BOOT_LINUX_RAMOOPS
static const struct linux_ramoops_config ramoops = {
    .addr = CONFIG_BOOT_LINUX_RAMOOPS_ADDR,
    .size = CONFIG_BOOT_LINUX_RAMOOPS_SIZE,
    .record_size = CONFIG_BOOT_LINUX_RAMOOPS_RECORD_SIZE,
    .console_size = CONFIG_BOOT_LINUX_RAMOOPS_CONSOLE_SIZE,
    .ftrace_size = CONFIG_BOOT_LINUX_RAMOOPS_FTRACE_SIZE,
    .pmsg_size = CONFIG_BOOT_LINUX_RAMOOPS_PMSG_SIZE,
};
#endif

int cm_error_to_wire(int error_code)
{
    switch (error_code) {
    case PB_OK:
        return PB_RESULT_OK;
    case -PB_ERR:
        return -PB_RESULT_ERROR;
    case -PB_ERR_TIMEOUT:
        return -PB_RESULT_TIMEOUT;
    case -PB_ERR_KEY_REVOKED:
        return -PB_RESULT_KEY_REVOKED;
    case -PB_ERR_SIGNATURE:
        return -PB_RESULT_SIGNATURE_ERROR;
    case -PB_ERR_CHECKSUM:
        return -PB_RESULT_ERROR;
    case -PB_ERR_MEM:
        return -PB_RESULT_MEM_ERROR;
    case -PB_ERR_IO:
        return -PB_RESULT_IO_ERROR;
    case -PB_ERR_NOT_FOUND:
        return -PB_RESULT_NOT_FOUND;
    case -PB_ERR_NOT_AUTHENTICATED:
        return -PB_RESULT_NOT_AUTHENTICATED;
    case -PB_ERR_AUTHENTICATION_FAILED:
        return -PB_RESULT_AUTHENTICATION_FAILED;
    case -PB_ERR_INVALID_ARGUMENT:
        return -PB_RESULT_INVALID_ARGUMENT;
    case -PB_ERR_INVALID_COMMAND:
        return -PB_RESULT_INVALID_COMMAND;
    case -PB_ERR_PART_NOT_FOUND:
        return -PB_RESULT_NOT_FOUND;
    case -PB_ERR_PART_NOT_BOOTABLE:
        return -PB_RESULT_PART_NOT_BOOTABLE;
    case -PB_ERR_PARAM:
        return -PB_RESULT_INVALID_ARGUMENT;
    case -PB_ERR_NOT_SUPPORTED:
        return -PB_RESULT_NOT_SUPPORTED;
    case -PB_ERR_BAD_PAYLOAD:
        return -PB_RESULT_BAD_PAYLOAD;
    default:
        return -PB_RESULT_ERROR;
    }
}

void cm_request_reset_on_success(void)
{
    reboot_requested = true;
}

/*
 * The session ends with the transport (command-mode.md 3.1): when it is
 * (re)started and when a transfer is lost, which is how the transport
 * reports an unplug and a bus reset (usb.md 2.4). The next host starts
 * without authentication and without a stream (5.4).
 */

static void cm_session_end(void)
{
#ifdef CONFIG_CM_AUTH
    cm_authenticated = false;
#else
    cm_authenticated = true;
#endif
    cm_stream_end();
}

/*
 * Waits for the transfer that was started, for at most
 * CONFIG_CM_TRANSFER_TIMEOUT, and kicks the watchdog while it waits
 * (command-mode.md 4.6): a host that stops in the middle of an exchange
 * neither holds pb for ever nor resets it.
 */
static int cm_transfer_wait(void)
{
    struct pb_timeout to;
    int rc;

    pb_timeout_init_us(&to, CONFIG_CM_TRANSFER_TIMEOUT * 1000000UL);

    do {
        plat_wdog_kick();
        rc = cm_transport_complete();

        if (rc == -PB_ERR_AGAIN && pb_timeout_has_expired(&to)) {
            LOG_ERR("Transfer timed out");
            rc = -PB_ERR_TIMEOUT;
        }
    } while (rc == -PB_ERR_AGAIN);

    return rc;
}

int cm_write(const void *buf, size_t length)
{
    int rc;

    if (transfer_lost)
        return -PB_ERR_IO;

    rc = cm_transport_write(buf, length);

    if (rc == PB_OK)
        rc = cm_transfer_wait();

    /* A transfer that failed ends the session (command-mode.md 4.4) */
    if (rc != PB_OK) {
        transfer_lost = true;
        cm_session_end();
    }

    return rc;
}

int cm_read(void *buf, size_t length)
{
    int rc;

    if (transfer_lost)
        return -PB_ERR_IO;

    rc = cm_transport_read(buf, length);

    if (rc == PB_OK)
        rc = cm_transfer_wait();

    /* A transfer that failed ends the session (command-mode.md 4.4) */
    if (rc != PB_OK) {
        transfer_lost = true;
        cm_session_end();
    }

    return rc;
}

/*
 * Reads a transfer of 'length' bytes that the host sends whatever the first
 * result said, and drops it: a refused command's exchange still ends as the
 * host expects (command-mode.md 3.2, 4.4). It is read in pieces into the
 * result, which the caller sets afterwards, so that no buffer changes.
 */
int cm_drain(size_t length)
{
    int rc;

    do {
        size_t chunk = (length > sizeof(cm_result)) ? sizeof(cm_result) : length;

        rc = cm_read(&cm_result, chunk);

        if (rc != PB_OK)
            return rc;

        length -= chunk;
    } while (length > 0);

    return PB_OK;
}

void cm_disconnect(void)
{
    if (!disconnected)
        cm_transport_disconnect();

    disconnected = true;
}

/*
 * A locked device requires authentication, and so does one whose life cycle
 * is unknown: the SLC was never set up (the board init failed before
 * slc_init()) or couldn't be read. Only a board without SLC support
 * (-PB_ERR_NOT_SUPPORTED) and the states before locking are open, and EOL
 * (D-011).
 */
static bool slc_requires_auth(void)
{
    if (cm_slc == PB_SLC_CONFIGURATION_LOCKED)
        return true;

    return (cm_slc < 0) && (cm_slc != -PB_ERR_NOT_SUPPORTED);
}

/*
 * The gate (command-mode.md 3.2), before anything looks at the frame. A
 * closed device runs two commands before authentication and answers every
 * other frame with NOT_AUTHENTICATED: also a number pb doesn't define and a
 * frame with a wrong magic, so that it doesn't tell which commands it has
 * (D-058).
 */
static bool cm_gate_allows(void)
{
    if (cm_authenticated || !slc_requires_auth())
        return true;

    return (cm_cmd.magic == PB_WIRE_MAGIC) && !pb_wire_requires_auth(&cm_cmd);
}

/*
 * The gate's refusal. Hosts send BOARD_COMMAND's request and
 * STREAM_PREPARE_BUFFER's data whatever the first result says, so the device
 * sends the first result, reads the data and leaves the second to the caller,
 * both NOT_AUTHENTICATED (command-mode.md 3.2, 4.4). Every other refused
 * frame is answered once.
 */
static void cm_gate_refuse(void)
{
    const struct pb_command_board *board_cmd = (const struct pb_command_board *)cm_cmd.request;
    const struct pb_command_stream_prepare_buffer *stream_prep =
        (const struct pb_command_stream_prepare_buffer *)cm_cmd.request;

    pb_wire_init_result(&cm_result, -PB_RESULT_NOT_AUTHENTICATED);

    if (cm_cmd.magic != PB_WIRE_MAGIC)
        return;

    if (cm_cmd.command == PB_CMD_BOARD_COMMAND) {
        cm_write(&cm_result, sizeof(cm_result));

        if (board_cmd->request_size)
            (void)cm_drain(CM_DATA_XFER_SIZE);

        pb_wire_init_result(&cm_result, -PB_RESULT_NOT_AUTHENTICATED);
    } else if (cm_cmd.command == PB_CMD_STREAM_PREPARE_BUFFER) {
        cm_write(&cm_result, sizeof(cm_result));
        (void)cm_drain(stream_prep->size);
        pb_wire_init_result(&cm_result, -PB_RESULT_NOT_AUTHENTICATED);
    }
}

static int pb_command_parse(void)
{
    int rc = PB_OK;

    if (!cm_gate_allows()) {
        LOG_ERR("Not authenticated");
        cm_gate_refuse();
        goto err_out;
    }

    if (!pb_wire_valid_command(&cm_cmd)) {
        LOG_ERR("Invalid command: %i\n", cm_cmd.command);
        pb_wire_init_result(&cm_result, -PB_RESULT_INVALID_COMMAND);
        goto err_out;
    }

    LOG_DBG("%i", cm_cmd.command);

    pb_wire_init_result(&cm_result, -PB_RESULT_NOT_SUPPORTED);

    switch (cm_cmd.command) {
    case PB_CMD_BOOTLOADER_VERSION_READ:
        rc = cm_cmd_version_read();
        break;
    case PB_CMD_DEVICE_RESET: {
        LOG_INFO("Board reset");
        pb_wire_init_result(&cm_result, PB_RESULT_OK);
        cm_write(&cm_result, sizeof(cm_result));
        cm_leave_requested = true;
        return -PB_ERR_ABORT;
    } break;
    case PB_CMD_PART_TBL_READ:
        rc = cm_cmd_part_tbl_read();
        break;
    case PB_CMD_DEVICE_READ_CAPS:
        rc = cm_cmd_read_caps();
        break;
    case PB_CMD_BOOT_BPAK:
        /* Sends its results itself: none is sent here */
        return cm_cmd_boot_bpak();
    case PB_CMD_PART_TBL_INSTALL: {
        /* In use, not deprecated: how a device gets its table (board.md
         * 2.1, 2.4, command-mode.md 5.3) */
        rc = cm_cmd_part_tbl_install((struct pb_command_install_part_table *)cm_cmd.request);
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    } break;
    case PB_CMD_STREAM_INITIALIZE:
        rc = cm_cmd_stream_init();
        break;
    case PB_CMD_STREAM_PREPARE_BUFFER:
        rc = cm_cmd_stream_prep_buffer();
        break;
    case PB_CMD_STREAM_READ_BUFFER:
        rc = cm_cmd_stream_read();
        break;
    case PB_CMD_STREAM_WRITE_BUFFER:
        rc = cm_cmd_stream_write();
        break;
    case PB_CMD_STREAM_FINALIZE:
        rc = cm_cmd_stream_final();
        break;
    case PB_CMD_PART_BPAK_READ:
        rc = cm_cmd_bpak_read();
        break;
    case PB_CMD_PART_VERIFY:
        rc = cm_cmd_part_verify();
        break;
    case PB_CMD_BOOT_PART:
        return cm_cmd_boot_part();
    case PB_CMD_DEVICE_IDENTIFIER_READ:
        rc = cm_cmd_device_identifier_read();
        break;
    case PB_CMD_PART_ACTIVATE:
        rc = cm_cmd_part_activate();
        break;
    case PB_CMD_AUTHENTICATE:
        rc = cm_cmd_auth();
        break;
    case PB_CMD_AUTH_SET_PASSWORD: {
        /* Reserved: never implemented (command-mode.md 5.8) */
        pb_wire_init_result(&cm_result, -PB_RESULT_NOT_SUPPORTED);
    } break;
    case PB_CMD_BOARD_COMMAND:
        rc = cm_cmd_board();
        break;
    case PB_CMD_COMMAND_LIST:
        rc = cm_cmd_command_list();
        break;
    case PB_CMD_BOARD_STATUS_READ:
        rc = cm_cmd_board_status();
        break;
    case PB_CMD_SLC_SET_CONFIGURATION:
        rc = cm_cmd_slc_set_configuration();
        break;
    case PB_CMD_SLC_SET_CONFIGURATION_LOCK:
        rc = cm_cmd_slc_set_configuration_lock();
        break;
    case PB_CMD_SLC_SET_EOL:
        rc = cm_cmd_slc_set_eol();
        break;
    case PB_CMD_SLC_REVOKE_KEY:
        rc = cm_cmd_slc_revoke_key();
        break;
    case PB_CMD_SLC_READ:
        rc = cm_cmd_slc_read();
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
        break;
    case PB_CMD_PART_RESIZE: {
        /* Retired before 1.0.0: the partition table's variants replaced
         * it. A retired command keeps its case and answers NOT_SUPPORTED,
         * behind the gate, and its number is never reused (command-mode.md
         * 5.8, compatibility.md) */
        pb_wire_init_result(&cm_result, -PB_RESULT_NOT_SUPPORTED);
    } break;
    case PB_CMD_BOOT_STATUS:
        rc = cm_cmd_boot_status();
        break;

    case PB_CMD_PART_ERASE: {
        struct pb_command_erase_part *erase_cmd = (struct pb_command_erase_part *)cm_cmd.request;

        rc = cm_cmd_part_erase(erase_cmd);
        pb_wire_init_result(&cm_result, cm_error_to_wire(rc));
    } break;
    case PB_CMD_LOG_READ:
        rc = cm_cmd_log_read();
        break;
    default: {
        LOG_ERR("Got unknown command: %u", cm_cmd.command);
        pb_wire_init_result(&cm_result, -PB_RESULT_INVALID_COMMAND);
    }
    }

err_out:
    cm_write(&cm_result, sizeof(cm_result));

    /* A named command asked for the reset (command-mode.md 5.6): only when
     * that command returned PB_OK. The request is dropped with a command that
     * failed, and doesn't wait for the next one that succeeds */
    if (reboot_requested) {
        reboot_requested = false;

        if (rc == PB_OK) {
            LOG_INFO("Requesting reboot...");
            cm_leave_requested = true;
            rc = -PB_ERR_ABORT;
        }
    }

    return rc;
}

int cm_init(const struct cm_config *cfg_)
{
    cm_cfg = cfg_;
    return PB_OK;
}

/* Where the task is (command-mode.md 2) */
enum cm_state {
    CM_SETUP, /* Its first call */
    CM_START, /* No session: the transport is to be started, or restarted */
    CM_CONNECT, /* No session: waiting for the transport (2.2) */
    CM_READ, /* A session: the read of the next command is to be started */
    CM_COMMAND, /* A session: waiting for a command */
};

static enum cm_state state = CM_SETUP;
/* The time the transport has to become ready, from when it was started */
static struct pb_timeout ready_timeout;

/* Microseconds in 32 bits, and neither 0 nor UINT32_MAX, which
 * pb_timeout_init_us() refuses: Kconfig's range, for a build that has its
 * configuration from elsewhere */
_Static_assert(CONFIG_CM_TRANSPORT_READY_TIMEOUT >= 1 &&
                   CONFIG_CM_TRANSPORT_READY_TIMEOUT <= (UINT32_MAX - 1) / 1000000,
               "CONFIG_CM_TRANSPORT_READY_TIMEOUT: 1 to 4294 s");

/*
 * What the platform recorded (platform.md 3.8), in the log, where a host
 * reads it without a UART: the state behind a boot that was refused, and
 * what a boot would go on without. Nothing of it is on the wire.
 */
static void log_plat_status(void)
{
    static const char *const filters[] = {
        [PLAT_TZ_DRAM_NONE] = "none",
        [PLAT_TZ_DRAM_LATER_STAGE] = "a later stage's",
        [PLAT_TZ_DRAM_ACTIVE] = "active",
        [PLAT_TZ_DRAM_BYPASSED] = "BYPASSED",
    };
    struct plat_status status;
    const char *filter = "unknown";

    /* What the platform leaves unset counts as failed, as for the boot */
    memset(&status, 0, sizeof(status));
    status.tz_dram_filter = PLAT_TZ_DRAM_BYPASSED;
    plat_get_status(&status);

    if ((size_t)status.tz_dram_filter < ARRAY_SIZE(filters))
        filter = filters[status.tz_dram_filter];

    LOG_INFO("Platform: DRAM filter %s, watchdog %s, crypto engine %s, ROM logs %s",
             filter,
             status.wdog_armed ? "armed" : "not armed",
             status.crypto_engine ? "ok" : "none",
             status.rom_log ? "found" : "none");
}

/* The set up, on the task's first call (command-mode.md 2.1) */
static int cm_setup(void)
{
    int rc;

    LOG_INFO("Initializing command mode");
    log_plat_status();

    /* The USB device port, first. Without it there is no command mode */
    rc = plat_cm_init();

    if (rc != PB_OK) {
        LOG_ERR("Platform init failed (%i)", rc);
        return rc;
    }

#ifdef CONFIG_BOOT_LINUX_RAMOOPS
    /* Only read through command mode, so not set up on the boot path. It
     * reads the region when the log is read. */
    rc = linux_ramoops_init(&ramoops);
    if (rc != PB_OK)
        LOG_ERR("Linux's ramoops log (%i)", rc);
#endif

    rc = cm_board_init();

    if (rc != PB_OK) {
        LOG_ERR("Board init failed (%i)", rc);
        return rc;
    }

    /* The named commands: a bad list is the build's error, no command mode
     * (command-mode.md 2.1, 5.6) */
    size_t commands;
    const struct cm_command *list = cm_commands(&commands);

    rc = cm_commands_check(list, commands, CM_BUF_SIZE);

    if (rc != PB_OK) {
        LOG_ERR("Named commands (%i)", rc);
        return rc;
    }

    cm_slc = slc_read_status();

    /* The device's identity: what DEVICE_IDENTIFIER_READ answers, and what a
     * token is a signature over (command-mode.md 3.3, platform.md 3.7). When
     * it can't be read there is no command mode, as for the set up's other
     * failures: a null UUID is no identity, and a token over it would be
     * every such device's */
    rc = device_uuid(cm_device_uu);

    if (rc != PB_OK) {
        LOG_ERR("Device UUID failed (%i)", rc);
        return rc;
    }

    return PB_OK;
}

/* Starts the transport, and starts it again after a session: from here the
 * transport has CONFIG_CM_TRANSPORT_READY_TIMEOUT to become ready (2.2) */
static int cm_start(void)
{
    int rc;

    /* A session starts with nothing: the last one ended with the transport
     * that is (re)started here (command-mode.md 3.1) */
    cm_session_end();
    transfer_lost = false;
    disconnected = false;

    rc = cm_transport_init();

    if (rc != PB_OK) {
        LOG_ERR("Transport init failed (%i)", rc);
        return -PB_ERR_IO;
    }

    LOG_INFO("Waiting for transport to become ready...");
    pb_timeout_init_us(&ready_timeout, CONFIG_CM_TRANSPORT_READY_TIMEOUT * 1000000UL);

    return PB_OK;
}

/* Leaves command mode (command-mode.md 2.3): the device leaves the bus, and
 * the error ends the loop of the tasks, so that pb resets. A caller that
 * runs the task again all the same finds it at its first call */
static int cm_leave(int rc)
{
    cm_disconnect();
    cm_leave_requested = false;
    state = CM_SETUP;
    return rc;
}

/*
 * Command mode's task (command-mode.md 2, board.md 4.5): a step of what
 * was a loop. A call never waits for the host: without a session it polls
 * the transport once, with one it polls the read of the next command once,
 * and it returns PB_OK to be called again, so that the other tasks run and
 * the core kicks the watchdog meanwhile. A command that has come runs to its
 * end inside the call, with its own kicks and its transfers' timeout (4.6).
 * An error ends the loop of the tasks: the set up failed, no host came in
 * time, a command asked for the reset, or a boot the host started returned
 * after the transport's disconnect.
 */
int cm_main_task(void)
{
    int rc;

    for (;;) {
        switch (state) {
        case CM_SETUP:
            rc = cm_setup();

            if (rc != PB_OK)
                return rc;

            state = CM_START;
            break;
        case CM_START:
            rc = cm_start();

            if (rc != PB_OK) {
                state = CM_SETUP;
                return rc;
            }

            state = CM_CONNECT;
            break;
        case CM_CONNECT:
            rc = cm_transport_connect();

            if (pb_timeout_has_expired(&ready_timeout))
                rc = -PB_ERR_TIMEOUT;

            if (rc == -PB_ERR_AGAIN)
                return PB_OK;

            if (rc != PB_OK) {
                LOG_ERR("Transport init error (%i)", rc);
                return cm_leave(rc);
            }

            LOG_INFO("Transport ready");
            state = CM_READ;
            break;
        case CM_READ:
            rc = cm_transport_read(&cm_cmd, sizeof(cm_cmd));

            /* Lost while no transfer ran is the error of the next one that
             * is started (usb.md 4.3): the session is over, the transport
             * is restarted and the device waits for a host (command-mode.md
             * 2.2, 3.1). In the next round, so that the tasks run and the
             * watchdog is kicked also when the start fails again */
            if (rc != PB_OK) {
                LOG_ERR("Read error %i", rc);
                state = CM_START;
                return PB_OK;
            }

            state = CM_COMMAND;
            return PB_OK;
        case CM_COMMAND:
            rc = cm_transport_complete();

            /* No command yet. A timeout is polled on (F-287) */
            if (rc == -PB_ERR_AGAIN || rc == -PB_ERR_TIMEOUT)
                return PB_OK;

            if (rc != PB_OK) {
                LOG_ERR("Read error %i", rc);
                state = CM_START;
                break;
            }

            rc = pb_command_parse();

            if (cm_leave_requested)
                return cm_leave(rc);

            if (transfer_lost) {
                /* The session is over: the transport is restarted and the
                 * device is back at waiting for a host (command-mode.md
                 * 3.1, 4.4, 4.6) */
                LOG_ERR("Transfer lost (%i), restarting the transport", rc);
                state = CM_START;
                break;
            }

            if (rc != PB_OK)
                LOG_ERR("Command error %i", rc);

            /* Read the next command */
            state = CM_READ;
            break;
        }
    }
}

DEFINE_TASK(cm_main, cm_main_task);
