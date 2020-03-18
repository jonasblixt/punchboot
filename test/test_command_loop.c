#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/protocol.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>
#include <pb-tools/slc.h>

#include "command.h"

static struct pb_command_ctx ctx;
static pthread_t thread;
static int fd;
static int client_fd;
static volatile bool ready;
static volatile bool run;
static bool device_config;
static bool device_config_lock;


#define SERVER_SOCK_FILE "/tmp/pb_test_sock"

static int pb_command_send_result(const struct pb_command_ctx *ctx,
                                    struct pb_result *result)
{
    ssize_t written = write(client_fd, result, sizeof(*result));

    if (written != sizeof(*result))
        return -PB_ERR;

    return PB_OK;
}

static int pb_command_reset(struct pb_command_ctx *ctx,
                            const struct pb_command *command,
                            struct pb_result *result)
{
    printf("Device reset command\n");
    pb_protocol_init_result(result, PB_RESULT_OK);

    return PB_OK;
}

static int pb_command_read_slc(struct pb_command_ctx *ctx,
                               const struct pb_command *command,
                               struct pb_result *result)
{
    uint32_t slc = PB_SLC_NOT_CONFIGURED;

    if (device_config_lock)
        slc = PB_SLC_CONFIGURATION_LOCKED;
    else if (device_config)
        slc = PB_SLC_CONFIGURATION;
    else
        slc = PB_SLC_NOT_CONFIGURED;

    printf("Read SLC\n");

    return pb_protocol_init_result2(result, PB_RESULT_OK, &slc, sizeof(slc));
}

static int pb_command_device_read_uuid(struct pb_command_ctx *ctx,
                                       const struct pb_command *command,
                                        struct pb_result *result)
{
    struct pb_result_device_identifier id;
    printf("Device read uuid command\n");

    uuid_parse("bd4475db-f4c4-454e-a4f1-156d99d0d0e5", id.device_uuid);
    snprintf(id.board_id, sizeof(id.board_id), "Test board");

    return pb_protocol_init_result2(result, PB_RESULT_OK, &id, sizeof(id));
}

static int pb_authenticate(struct pb_command_ctx *ctx,
                           const struct pb_command *command,
                           struct pb_result *result)
{
    struct pb_command_authenticate *auth = \
                     (struct pb_command_authenticate *) command->request;

    const char auth_cookie[] = "0123456789abcdef";
    uint8_t auth_data[256];


    ctx->authenticated = false;
    printf("Got auth request method: %i, size: %i\n", auth->method,
                                                      auth->size);

    pb_protocol_init_result(result, PB_RESULT_ERROR);

    ssize_t bytes = read(client_fd, auth_data, auth->size);

    if (bytes != auth->size)
    {
        printf("Error: bytes != auth->size\n");
        return -PB_ERR;
    }

    if (auth->method != PB_AUTH_PASSWORD)
    {
        printf("Error: auth->method != PB_AUTH_PASSWORD\n");
        return -PB_ERR;
    }

    if (memcmp(auth_cookie, auth_data, auth->size) == 0)
    {
        printf("Auth OK\n");
        ctx->authenticated = true;
        pb_protocol_init_result(result, PB_RESULT_OK);
        return PB_OK;
    }

    return -PB_ERR;
}

static int pb_command_config(struct pb_command_ctx *ctx,
                             const struct pb_command *command,
                             struct pb_result *result)
{
    if (device_config_lock)
    {
        pb_protocol_init_result(result, PB_RESULT_ERROR);
        return -PB_ERR;
    }

    pb_protocol_init_result(result, PB_RESULT_OK);
    device_config = true;
    return PB_OK;
}

static int pb_command_config_lock(struct pb_command_ctx *ctx,
                                  const struct pb_command *command,
                                  struct pb_result *result)
{
    if (!device_config)
    {
        printf("Error: Trying to lock un-configured device\n");
        pb_protocol_init_result(result, PB_RESULT_ERROR);
        return -PB_ERR;
    }

    if (device_config_lock)
    {
        printf("Error: already locked\n");
        pb_protocol_init_result(result, PB_RESULT_ERROR);
        return -PB_ERR;
    }

    pb_protocol_init_result(result, PB_RESULT_OK);
    device_config_lock = true;
    return PB_OK;
}

static void * test_command_loop(void *arg)
{
    int rc;
    struct pb_command command;
    struct sockaddr_un addr;
    struct sockaddr_un from;
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
}

static bool force_auth = false;

int test_command_loop_start(struct pb_command_ctx **ctxp)
{
    int rc;

    run = true;
    ready = false;

    rc = pb_command_init(&ctx, pb_command_send_result, NULL, NULL);

    if (rc != PB_OK)
        return -PB_ERR;

    ctx.authenticated = force_auth;

    pb_command_configure(&ctx, PB_CMD_DEVICE_RESET, pb_command_reset);
    pb_command_configure(&ctx, PB_CMD_DEVICE_SLC_READ, pb_command_read_slc);
    pb_command_configure(&ctx, PB_CMD_DEVICE_CONFIG, &pb_command_config);
    pb_command_configure(&ctx, PB_CMD_DEVICE_CONFIG_LOCK, &pb_command_config_lock);
    pb_command_configure(&ctx, PB_CMD_DEVICE_IDENTIFIER_READ,
                                        pb_command_device_read_uuid);
    pb_command_configure(&ctx, PB_CMD_AUTHENTICATE,
                                        pb_authenticate);

    rc = pthread_create(&thread, NULL, test_command_loop, NULL);

    while (!ready) {}

    if (ctxp)
    {
        *ctxp = &ctx;
    }

    return PB_OK;
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

    return PB_OK;
}

