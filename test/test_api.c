#include "nala.h"
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>

#include "test_command_loop.h"
#include "command.h"

TEST(api_init)
{
    int rc;
    struct pb_context *ctx;

    rc = pb_api_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    remove("/tmp/pb_test_sock");
    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}

TEST(api_reset)
{
    int rc;
    struct pb_context *ctx;

    test_command_loop_set_authenticated(true);

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_OK);


    /* Create command context and connect */
    rc = pb_api_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Send device reset command */
    rc = pb_api_device_reset(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}

TEST(api_unsupported_function)
{
    int rc;
    struct pb_context *ctx;
    struct pb_command_ctx *command_ctx;

    test_command_loop_set_authenticated(true);

    /* Start command loop */
    rc = test_command_loop_start(&command_ctx);
    ASSERT_EQ(rc, PB_OK);


    rc = pb_command_configure(command_ctx, PB_CMD_DEVICE_RESET, NULL);
    ASSERT_EQ(rc, PB_OK);

    /* Create command context and connect */
    rc = pb_api_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Send device reset command */
    rc = pb_api_device_reset(ctx);
    ASSERT_EQ(rc, -PB_NOT_SUPPORTED);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}


TEST(api_read_device_uuid)
{
    int rc;
    char uuid_tmp[37];
    struct pb_context *ctx;
    uint8_t device_uuid_raw[16];
    char board_id[16];

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_OK);

    test_command_loop_set_authenticated(true);

    /* Create command context and connect */
    rc = pb_api_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Send command */
    rc = pb_api_device_read_identifier(ctx, device_uuid_raw, sizeof(device_uuid_raw),
                                            board_id, sizeof(board_id));
    ASSERT_EQ(rc, PB_OK);

    uuid_unparse(device_uuid_raw, uuid_tmp);
    printf("Device uuid: %s\n", uuid_tmp);
    printf("Board: %s\n", board_id);

    uuid_t device_uuid;

    uuid_parse("bd4475db-f4c4-454e-a4f1-156d99d0d0e5", device_uuid);

    ASSERT_EQ(uuid_compare(device_uuid, device_uuid_raw), 0);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}
