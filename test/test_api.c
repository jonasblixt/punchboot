#include "nala.h"
#include <uuid.h>
#include <pb/api.h>
#include <pb/command.h>
#include <pb/error.h>
#include <pb/socket.h>

#include "test_command_loop.h"

TEST(api_init)
{
    int rc;
    struct pb_context *ctx;

    rc = pb_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    remove("/tmp/pb_test_sock");
    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = pb_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}

TEST(api_reset)
{
    int rc;
    struct pb_context *ctx;

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_OK);

    test_command_loop_set_authenticated(true);

    /* Create command context and connect */
    rc = pb_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Send device reset command */
    rc = pb_device_reset(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}

TEST(api_unsupported_function)
{
    int rc;
    struct pb_context *ctx;
    struct pb_command_ctx *command_ctx;

    /* Start command loop */
    rc = test_command_loop_start(&command_ctx);
    ASSERT_EQ(rc, PB_OK);

    test_command_loop_set_authenticated(true);
    command_ctx->device_reset = NULL;

    /* Create command context and connect */
    rc = pb_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Send device reset command */
    rc = pb_device_reset(ctx);
    ASSERT_EQ(rc, -PB_ERR);

    ASSERT_EQ(ctx->last_result.result_code, PB_RESULT_NOT_SUPPORTED);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}


TEST(api_read_device_uuid)
{
    int rc;
    char uuid_tmp[37];
    struct pb_result_device_uuid uu;
    struct pb_context *ctx;

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_OK);

    test_command_loop_set_authenticated(true);

    /* Create command context and connect */
    rc = pb_create_context(&ctx);
    ASSERT_EQ(rc, PB_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_OK);

    /* Send command */
    rc = pb_device_read_uuid(ctx, &uu);
    ASSERT_EQ(rc, PB_OK);

    uuid_unparse(uu.device_uuid, uuid_tmp);
    printf("Device uuid: %s\n", uuid_tmp);

    uuid_unparse(uu.platform_uuid, uuid_tmp);
    printf("Platform uuid: %s\n", uuid_tmp);

    uuid_t device_uuid, platform_uuid;

    uuid_parse("bd4475db-f4c4-454e-a4f1-156d99d0d0e5", device_uuid);
    uuid_parse("2ba102a6-d9b3-4b4f-80b9-2512b252f773", platform_uuid);

    ASSERT_EQ(uuid_compare(device_uuid, uu.device_uuid), 0);
    ASSERT_EQ(uuid_compare(platform_uuid, uu.platform_uuid), 0);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}
