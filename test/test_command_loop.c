#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "../src/uuid/uuid.h"
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>

#include "command.h"

static struct pb_command_ctx ctx;
static pthread_t thread;
static int fd;
static int client_fd;
static volatile bool ready;
static volatile bool run;
static bool device_config;
static bool device_config_lock;
static char password[64];
static struct pb_result_slc_key_status key_status =
    {
        .active = {0x02020202, 0x03030303, 0x04040404},
        .revoked = {0x01010101},
    };
static struct pb_result_part_table_entry tbl[128];
static uint8_t stream_buffer[2][8192];
static const uint32_t chunk_size = 256;

#define SERVER_SOCK_FILE "/tmp/pb_test_sock"

static int pb_command_send_result(const struct pb_command_ctx *ctx,
                                    struct pb_result *result)
{
    ssize_t written = write(client_fd, result, sizeof(*result));

    if (written != sizeof(*result))
        return -PB_RESULT_ERROR;

    return PB_RESULT_OK;
}

static int pb_command_reset(struct pb_command_ctx *ctx,
                            const struct pb_command *command,
                            struct pb_result *result)
{
    printf("Device reset command\n");
    pb_wire_init_result(result, PB_RESULT_OK);

    return PB_RESULT_OK;
}

static int pb_read_caps(struct pb_command_ctx *ctx,
                            const struct pb_command *command,
                            struct pb_result *result)
{
    struct pb_result_device_caps caps;

    printf("Read caps command\n");
    caps.stream_no_of_buffers = 2;
    caps.stream_buffer_size = 1024*1024*4;
    caps.operation_timeout_ms = 1000;
    caps.part_erase_timeout_ms = 30000;
    caps.chunk_transfer_max_bytes = chunk_size;

    pb_wire_init_result2(result, PB_RESULT_OK, &caps, sizeof(caps));

    return PB_RESULT_OK;
}

static int pb_command_read_slc(struct pb_command_ctx *ctx,
                               const struct pb_command *command,
                               struct pb_result *result)
{
    struct pb_result_slc slc;
    struct pb_result slc_result;

    ssize_t written;
    slc.slc = PB_SLC_NOT_CONFIGURED;

    if (device_config_lock)
        slc.slc = PB_SLC_CONFIGURATION_LOCKED;
    else if (device_config)
        slc.slc = PB_SLC_CONFIGURATION;
    else
        slc.slc = PB_SLC_NOT_CONFIGURED;

    printf("Read SLC %i\n", slc.slc);

    pb_wire_init_result2(&slc_result, PB_RESULT_OK, &slc, sizeof(slc));

    ctx->send_result(ctx, &slc_result);

    written = write(client_fd, &key_status, sizeof(key_status));

    if (written != sizeof(key_status))
        return -PB_RESULT_ERROR;

    return pb_wire_init_result(result, PB_RESULT_OK);
}

static int pb_command_device_read_uuid(struct pb_command_ctx *ctx,
                                       const struct pb_command *command,
                                        struct pb_result *result)
{
    struct pb_result_device_identifier id;
    printf("Device read uuid command\n");

    uuid_parse("bd4475db-f4c4-454e-a4f1-156d99d0d0e5", id.device_uuid);
    snprintf(id.board_id, sizeof(id.board_id), "Test board");

    return pb_wire_init_result2(result, PB_RESULT_OK, &id, sizeof(id));
}

static int pb_authenticate(struct pb_command_ctx *ctx,
                           const struct pb_command *command,
                           struct pb_result *result)
{
    struct pb_command_authenticate *auth = \
                     (struct pb_command_authenticate *) command->request;

    uint8_t auth_data[1024];


    ctx->authenticated = false;
    printf("Got auth request method: %i, size: %i\n", auth->method,
                                                      auth->size);

    pb_wire_init_result(result, PB_RESULT_OK);

    ctx->send_result(ctx, result);

    pb_wire_init_result(result, -PB_RESULT_ERROR);

    ssize_t bytes = read(client_fd, auth_data, sizeof(auth_data));

    if (bytes < 0) {
        printf("%s: Error reading %li\n", __func__, bytes);
        return -PB_RESULT_ERROR;
    }
/*
    if (bytes != auth->size)
    {
        printf("Error: bytes != auth->size\n");
        return -PB_RESULT_ERROR;
    }
*/
    if (auth->method != PB_AUTH_PASSWORD)
    {
        printf("Error: auth->method != PB_AUTH_PASSWORD\n");
        return -PB_RESULT_ERROR;
    }

    if (memcmp(password, auth_data, auth->size) == 0)
    {
        printf("Auth OK\n");
        ctx->authenticated = true;
        pb_wire_init_result(result, PB_RESULT_OK);
        return PB_RESULT_OK;
    }

    return pb_wire_init_result(result, -PB_RESULT_AUTHENTICATION_FAILED);
}

static int pb_command_config(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    if (device_config_lock)
    {
        pb_wire_init_result(result, -PB_RESULT_ERROR);
        return -PB_RESULT_ERROR;
    }

    pb_wire_init_result(result, PB_RESULT_OK);
    device_config = true;
    return PB_RESULT_OK;
}


static int pb_revoke_key(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    struct pb_command_revoke_key revoke;
    int rc = -PB_RESULT_ERROR;

    memcpy(&revoke, command->request, sizeof(revoke));

    printf("Revoking key %x\n", revoke.key_id);

    for (int i = 0; i < 16; i++)
    {
        if (key_status.active[i] == revoke.key_id)
        {

            for (int n = 0; n < 16; n++)
            {
                if (!key_status.revoked[n])
                {
                    key_status.revoked[n] = key_status.active[i];
                    break;
                }
            }

            key_status.active[i] = 0;
            rc = PB_RESULT_OK;
            break;
        }
    }


    pb_wire_init_result(result, rc);
    return PB_RESULT_OK;
}

static int pb_read_version(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    const char *version = "1.0.0-dev+0123123";
    return pb_wire_init_result2(result, PB_RESULT_OK, (char *) version,
                                    strlen(version));
}


static int pb_board_cmd(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;
    uint8_t buf[1024];
    char *response = "Hello";

    struct pb_result_board board_result;

    printf("Board command\n");

    memset(&board_result, 0, sizeof(board_result));

    board_result.size = strlen(response);

    pb_wire_init_result(result, PB_RESULT_OK);

    rc = ctx->send_result(ctx, result);

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Could not send result\n");
        return rc;
    }
    ssize_t bytes = read(client_fd, buf, sizeof(buf));

    pb_wire_init_result2(result, PB_RESULT_OK, &board_result,
                            sizeof(board_result));

    rc = ctx->send_result(ctx, result);

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Could not send result\n");
        return rc;
    }

    rc = PB_RESULT_OK;

    bytes = write(client_fd, response, strlen(response));

    if (bytes != strlen(response))
    {
        printf("Error: Could not write response %li\n", bytes);
        rc = -PB_RESULT_ERROR;
    }

    return pb_wire_init_result(result, rc);
}

static int pb_board_status(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;
    char *response = "Hello";

    struct pb_result_board_status status_result;

    printf("Board status\n");

    memset(&status_result, 0, sizeof(status_result));

    status_result.size = strlen(response);

    pb_wire_init_result2(result, PB_RESULT_OK, &status_result,
                            sizeof(status_result));

    rc = ctx->send_result(ctx, result);

    if (rc != PB_RESULT_OK)
    {
        printf("Error: Could not send result\n");
        return rc;
    }

    rc = PB_RESULT_OK;

    ssize_t bytes = write(client_fd, response, strlen(response));

    if (bytes != strlen(response))
    {
        printf("Error: Could not write response %li\n", bytes);
        rc = -PB_RESULT_ERROR;
    }

    return pb_wire_init_result(result, rc);
}

static int pb_boot_ram(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;
    struct bpak_header header;
    uint8_t buf[1024];

    printf("Boot ram req\n");

    pb_wire_init_result(result, PB_RESULT_OK);

    rc = ctx->send_result(ctx, result);
    ssize_t bytes = read(client_fd, &header, sizeof(header));

    if (bytes != sizeof(header))
    {
        rc = -PB_RESULT_ERROR;
        goto err_out;
    }

    rc = bpak_valid_header(&header);

    if (rc != BPAK_OK)
    {
        rc = -PB_RESULT_ERROR;
        goto err_out;
    }

    printf("Found valid bpak header\n");
    pb_wire_init_result(result, PB_RESULT_OK);

    rc = ctx->send_result(ctx, result);

    bpak_foreach_part(&header, p)
    {
        if (!p->id)
            break;

        printf("loading part %x, %li bytes\n", p->id, bpak_part_size(p));
        size_t bytes_to_transfer = bpak_part_size(p);
        size_t chunk = 0;

        while (bytes_to_transfer)
        {
            chunk = bytes_to_transfer > chunk_size?chunk_size:bytes_to_transfer;
            printf("reading chunk %li\n", chunk);
            bytes = read(client_fd, buf, chunk);
            bytes_to_transfer -= chunk;
        }

        printf("Sending result\n");
        pb_wire_init_result(result, PB_RESULT_OK);
        ctx->send_result(ctx, result);
    }

err_out:
    return pb_wire_init_result(result, rc);
}

static int pb_boot_part(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;
    struct pb_command_boot_part *boot_command = \
       (struct pb_command_boot_part *) command->request;



    printf("Boot part req\n");

    for (int i = 0; i < 128; i++)
    {
        if (tbl[i].block_size == 0)
            break;

        if (memcmp(tbl[i].uuid, boot_command->uuid, 16) == 0)
        {
            printf("Found part\n");

            if (tbl[i].flags & PB_PART_FLAG_BOOTABLE)
                rc = PB_RESULT_OK;
            else
                rc = -PB_RESULT_PART_NOT_BOOTABLE;
        }
    }

    return pb_wire_init_result(result, rc);
}


static bool stream_configured = false;
static uint64_t stream_count = 0;

static int pb_stream_init(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;

    printf("stream init\n");
    stream_configured = true;
    stream_count = 0;

    rc = PB_RESULT_OK;

    return pb_wire_init_result(result, rc);
}

static int pb_stream_fin(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;


    printf("stream fin\n");
    stream_configured = false;
    stream_count = 0;

    rc = PB_RESULT_OK;

    return pb_wire_init_result(result, rc);
}

static int pb_stream_buffer_prep(struct pb_command_ctx *ctx,
                                 const struct pb_command *command,
                                 struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;
    struct pb_command_stream_prepare_buffer *prep_command = \
       (struct pb_command_stream_prepare_buffer *) command->request;



    printf("stream buffer prep\n");

    if (!stream_configured)
    {
        rc = -PB_RESULT_ERROR;
        pb_wire_init_result(result, rc);
        ctx->send_result(ctx, result);
        return rc;
    }


    if (prep_command->id > 1)
    {
        rc = -PB_RESULT_INVALID_ARGUMENT;
        pb_wire_init_result(result, rc);
        ctx->send_result(ctx, result);
        return rc;
    }

    pb_wire_init_result(result, PB_RESULT_OK);
    rc = ctx->send_result(ctx, result);

    printf("Reading data... %i %i\n", prep_command->size, prep_command->id);
    ssize_t bytes = read(client_fd, stream_buffer[prep_command->id],
                            prep_command->size);
    printf("Done %li\n", bytes);

    stream_count += prep_command->size;

    return pb_wire_init_result(result, rc);
};


static int pb_stream_buffer_write(struct pb_command_ctx *ctx,
                                 const struct pb_command *command,
                                 struct pb_result *result)
{
    int rc = -PB_RESULT_ERROR;
    struct pb_command_stream_write_buffer *write_command = \
       (struct pb_command_stream_write_buffer *) command->request;



    printf("stream buffer write buf:%i offset:%li size:%i\n",
        write_command->buffer_id, write_command->offset, write_command->size);
    rc = PB_RESULT_OK;

    return pb_wire_init_result(result, rc);
};

static int pb_read_bpak(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = PB_RESULT_OK;
    struct pb_result cmd_result;
    struct bpak_header h;

    printf("Read bpak header\n");
    pb_wire_init_result(&cmd_result, rc);
    ctx->send_result(ctx, &cmd_result);

    bpak_init_header(&h);
    (void) write(client_fd, &h, sizeof(h));

    return pb_wire_init_result(result, PB_RESULT_OK);
}

static int pb_install_part_table(struct pb_command_ctx *ctx,
                                 const struct pb_command *command,
                                 struct pb_result *result)
{
    struct pb_result_part_table_entry tbl_to_install[2] =
    {
        {
            .uuid = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe0",
            .description = "Boot 0",
            .first_block = 0,
            .last_block = 1,
            .block_size = 512,
            .flags = PB_PART_FLAG_WRITABLE,
        },
        {
            .uuid = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe1",
            .description = "System 0",
            .first_block = 0,
            .last_block = 2047,
            .block_size = 512,
            .flags = PB_PART_FLAG_WRITABLE | PB_PART_FLAG_BOOTABLE,
        },
    };

    memset(tbl, 0, sizeof(tbl));
    memcpy(tbl, tbl_to_install, sizeof(tbl_to_install));

    return pb_wire_init_result(result, PB_RESULT_OK);
};

static int pb_read_part_table(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc;
    struct pb_result cmd_result;
    struct pb_result_part_table_read read_result;
    int c = 0;

    for (c = 0; c < 128; c++)
    {
        if (!tbl[c].block_size)
            break;
    }

    read_result.no_of_entries = c;

    rc = pb_wire_init_result2(&cmd_result, PB_RESULT_OK, &read_result,
                                    sizeof(read_result));

    if (rc != PB_RESULT_OK)
    {
        printf("Result init failed\n");
    }

    printf("Sending result...");
    rc = ctx->send_result(ctx, &cmd_result);
    printf("  %i\n", rc);
    ssize_t bytes = write(client_fd, tbl,
                        sizeof(struct pb_result_part_table_entry)*c);

    if (bytes != sizeof(struct pb_result_part_table_entry)*c)
    {
        return -PB_RESULT_ERROR;
    }

    return pb_wire_init_result(result, PB_RESULT_OK);
}

static int pb_verify_part(struct pb_command_ctx *ctx,
                          const struct pb_command *command,
                          struct pb_result *result)
{
    int rc = -PB_RESULT_INVALID_ARGUMENT;

    struct pb_command_verify_part verify;

    memcpy(&verify, command->request, sizeof(verify));

    printf("Got verify request\n");

    for (int i = 0; i < 128; i++)
    {
        if (tbl[i].block_size == 0)
            break;

        if (memcmp(tbl[i].uuid, verify.uuid, 16) == 0)
        {
            printf("Found part\n");

            if (memcmp(verify.sha256, "hejhopp123", 10) == 0)
                rc = PB_RESULT_OK;
            else
                rc = -PB_RESULT_PART_VERIFY_FAILED;

            break;
        }
    }


    return pb_wire_init_result(result, rc);
}


static int pb_activate_part(struct pb_command_ctx *ctx,
                          const struct pb_command *command,
                          struct pb_result *result)
{
    int rc = -PB_RESULT_INVALID_ARGUMENT;

    struct pb_command_activate_part activate;

    memcpy(&activate, command->request, sizeof(activate));

    printf("Got activate request\n");

    for (int i = 0; i < 128; i++)
    {
        if (tbl[i].block_size == 0)
            break;

        if (memcmp(tbl[i].uuid, activate.uuid, 16) == 0)
        {
            printf("Found part\n");

            if (tbl[i].flags & PB_PART_FLAG_BOOTABLE)
                rc = PB_RESULT_OK;
            else
                rc = -PB_RESULT_PART_NOT_BOOTABLE;

            break;
        }
    }


    return pb_wire_init_result(result, rc);
}

static int pb_set_otp_password(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    int rc = PB_RESULT_OK;

    char *new_pass = (char *) command->request;

    if (strlen(password))
    {
        printf("Error OTP password already set\n");
        rc = -PB_RESULT_ERROR;
        goto err_out;
    }

    memcpy(password, new_pass, strlen(new_pass));

err_out:
    return pb_wire_init_result(result, rc);
}

static int pb_command_config_lock(struct pb_command_ctx *ctx,
                                  const struct pb_command *command,
                                  struct pb_result *result)
{
    if (!device_config)
    {
        printf("Error: Trying to lock un-configured device\n");
        pb_wire_init_result(result, -PB_RESULT_ERROR);
        return -PB_RESULT_ERROR;
    }

    if (device_config_lock)
    {
        printf("Error: already locked\n");
        pb_wire_init_result(result, -PB_RESULT_ERROR);
        return -PB_RESULT_ERROR;
    }

    pb_wire_init_result(result, PB_RESULT_OK);
    device_config_lock = true;
    return PB_RESULT_OK;
}

static void * test_command_loop(void *arg)
{
    struct pb_command command;
    struct sockaddr_un addr;
    ssize_t len;


    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        return NULL;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    strncpy(addr.sun_path, SERVER_SOCK_FILE,
                sizeof(addr.sun_path)-1);

    unlink(SERVER_SOCK_FILE);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return NULL;
    }

    ready = true;

    listen(fd, 5);

    printf("Command loop thread started\n");

    client_fd = accept(fd, NULL, NULL);

    while (run)
    {
        len = read(client_fd, &command, sizeof(command));

        if (len == sizeof(command))
            pb_command_process(&ctx, &command);
    }

    printf("Command loop stopped\n");
    return 0;
}

static bool force_auth = false;

int test_command_loop_start(struct pb_command_ctx **ctxp)
{
    int rc;

    run = true;
    ready = false;

    rc = pb_command_init(&ctx, pb_command_send_result, NULL, NULL);

    if (rc != PB_RESULT_OK)
        return -PB_RESULT_ERROR;

    ctx.authenticated = force_auth;

    pb_command_configure(&ctx, PB_CMD_DEVICE_RESET, pb_command_reset);
    pb_command_configure(&ctx, PB_CMD_SLC_READ, pb_command_read_slc);
    pb_command_configure(&ctx, PB_CMD_SLC_SET_CONFIGURATION,
                                            &pb_command_config);
    pb_command_configure(&ctx, PB_CMD_SLC_SET_CONFIGURATION_LOCK,
                                            &pb_command_config_lock);
    pb_command_configure(&ctx, PB_CMD_SLC_REVOKE_KEY, &pb_revoke_key);

    pb_command_configure(&ctx, PB_CMD_DEVICE_IDENTIFIER_READ,
                                        pb_command_device_read_uuid);
    pb_command_configure(&ctx, PB_CMD_AUTHENTICATE,
                                        pb_authenticate);

    pb_command_configure(&ctx, PB_CMD_DEVICE_READ_CAPS,
                                        pb_read_caps);

    pb_command_configure(&ctx, PB_CMD_AUTH_SET_OTP_PASSWORD,
                                        pb_set_otp_password);


    pb_command_configure(&ctx, PB_CMD_BOOTLOADER_VERSION_READ,
                                        pb_read_version);

    pb_command_configure(&ctx, PB_CMD_PART_TBL_READ,
                                        pb_read_part_table);

    pb_command_configure(&ctx, PB_CMD_PART_TBL_INSTALL,
                                        pb_install_part_table);

    pb_command_configure(&ctx, PB_CMD_PART_VERIFY,
                                        pb_verify_part);

    pb_command_configure(&ctx, PB_CMD_PART_ACTIVATE, pb_activate_part);

    pb_command_configure(&ctx, PB_CMD_PART_BPAK_READ, pb_read_bpak);

    pb_command_configure(&ctx, PB_CMD_STREAM_INITIALIZE, pb_stream_init);

    pb_command_configure(&ctx, PB_CMD_STREAM_PREPARE_BUFFER,
                                                pb_stream_buffer_prep);

    pb_command_configure(&ctx, PB_CMD_STREAM_WRITE_BUFFER,
                                                pb_stream_buffer_write);

    pb_command_configure(&ctx, PB_CMD_STREAM_FINALIZE, pb_stream_fin);

    pb_command_configure(&ctx, PB_CMD_BOOT_PART, pb_boot_part);

    pb_command_configure(&ctx, PB_CMD_BOOT_RAM, pb_boot_ram);

    pb_command_configure(&ctx, PB_CMD_BOARD_COMMAND, pb_board_cmd);

    pb_command_configure(&ctx, PB_CMD_BOARD_STATUS_READ, pb_board_status);

    rc = pthread_create(&thread, NULL, test_command_loop, NULL);

    while (!ready) {}

    if (ctxp)
    {
        *ctxp = &ctx;
    }

    return PB_RESULT_OK;
}

void test_command_loop_set_authenticated(bool authenticated)
{
   force_auth = authenticated;
}

int test_command_loop_stop(void)
{
    run = false;

    shutdown(client_fd, SHUT_RDWR);
    shutdown(fd, SHUT_RDWR);
    close(client_fd);
    close(fd);
    pthread_join(thread, NULL);

    return PB_RESULT_OK;
}

