#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <uuid.h>
#include <pb/api.h>
#include <pb/protocol.h>
#include <pb/error.h>
#include <pb/socket.h>
#include <pb/command.h>

static struct pb_command_ctx ctx;
static pthread_t thread;
static int fd;
static int client_fd;
static volatile bool ready;
static volatile bool run;
static bool device_config;
static bool device_config_lock;


#define SERVER_SOCK_FILE "/tmp/pb_test_sock"

static int pb_command_send_result(struct pb_command_ctx *ctx,
                                    struct pb_result *result)
{
    ssize_t written = write(client_fd, result, sizeof(*result));

    if (written != sizeof(*result))
        return -PB_ERR;

    return PB_OK;
}

static int pb_command_reset(struct pb_command_ctx *ctx)
{
    printf("Device reset command\n");
    return PB_OK;
}

static int pb_command_read_slc(struct pb_command_ctx *ctx,
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
                                        struct pb_result *result)
{
    struct pb_result_device_uuid uu;
    printf("Device read uuid command\n");

    uuid_parse("bd4475db-f4c4-454e-a4f1-156d99d0d0e5", uu.device_uuid);
    uuid_parse("2ba102a6-d9b3-4b4f-80b9-2512b252f773", uu.platform_uuid);
    return pb_protocol_init_result2(result, PB_RESULT_OK, &uu, sizeof(uu));
}

static int pb_authenticate(struct pb_command_ctx *ctx,
                           struct pb_command *command)
{
    struct pb_result result;
    struct pb_command_authenticate *auth = \
                     (struct pb_command_authenticate *) command->request;

    const char auth_cookie[] = "0123456789abcdef";
    uint8_t auth_data[256];


    printf("Got auth request method: %i, size: %i\n", auth->method,
                                                      auth->size);


    ssize_t bytes = read(client_fd, auth_data, auth->size);

    if (bytes != auth->size)
        return -PB_ERR;

    if (auth->method != PB_AUTH_PASSWORD)
        return -PB_ERR;

    if (memcmp(auth_cookie, auth_data, auth->size) == 0)
        return PB_OK;

    return -PB_ERR;
}

static int pb_command_config(struct pb_command_ctx *ctx,
                             struct pb_command *command)
{
    if (device_config_lock)
        return -PB_ERR;

    device_config = true;
    return PB_OK;
}

static int pb_command_config_lock(struct pb_command_ctx *ctx,
                             struct pb_command *command)
{
    if (!device_config)
    {
        printf("Error: Trying to lock un-configured device\n");
        return -PB_ERR;
    }

    if (device_config_lock)
    {
        printf("Error: already locked\n");
        return -PB_ERR;
    }
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

    memset(&ctx, 0, sizeof(ctx));
    ctx.device_reset = pb_command_reset;
    ctx.send_result = pb_command_send_result;
    ctx.device_read_uuid = pb_command_device_read_uuid;
    ctx.authenticate = pb_authenticate;
    ctx.read_slc = pb_command_read_slc;
    ctx.device_config = pb_command_config;
    ctx.device_config_lock = pb_command_config_lock;

    rc = pb_command_init(&ctx);

    if (rc != PB_OK)
        return NULL;

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

int test_command_loop_start(struct pb_command_ctx **ctxp)
{
    int rc;

    run = true;
    ready = false;

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
    ctx.authenticated = authenticated;
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

