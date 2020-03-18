#include <string.h>
#include "nala.h"
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>

#include "test_command_loop.h"
#include "command.h"


TEST(api_auth_fail)
{
    int rc;
    struct pb_context *ctx;

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
    ASSERT_EQ(rc, -PB_NOT_AUTHENTICATED);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}

TEST(api_auth_ok)
{
    int rc;
    struct pb_context *ctx;
    const char auth_cookie[] = "0123456789abcdef";

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

    /* Authenticate */
    rc = pb_api_authenticate_password(ctx,
                             (char *) auth_cookie, strlen(auth_cookie));
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
