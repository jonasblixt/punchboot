#include <string.h>
#include "nala.h"
#include "../src/uuid/uuid.h"
#include <pb-tools/api.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>

#include "test_command_loop.h"
#include "command.h"
#include "common.h"

TEST(api_auth_fail)
{
    int rc;
    struct pb_context *ctx;

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Create command context and connect */
    rc = pb_api_create_context(&ctx, pb_test_debug);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Send device reset command */
    rc = pb_api_device_reset(ctx);
    ASSERT_EQ(rc, -PB_RESULT_NOT_AUTHENTICATED);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(api_auth_fail2)
{
    int rc;
    struct pb_context *ctx;
    const char password[] = "0123456789abcdef";

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Create command context and connect */
    rc = pb_api_create_context(&ctx, pb_test_debug);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_api_auth_set_otp_password(ctx, password, strlen(password));
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Authenticate, should fail, incorrect password */
    rc = pb_api_authenticate_password(ctx,
                             (uint8_t *) "pelle", 5);
    ASSERT_EQ(rc, -PB_RESULT_AUTHENTICATION_FAILED);



    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(api_auth_set_otp_pass)
{
    int rc;
    struct pb_context *ctx;
    const char password[] = "0123456789abcdef";

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Create command context and connect */
    rc = pb_api_create_context(&ctx, pb_test_debug);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Authenticate, should fail since no password is set yet  */
    rc = pb_api_authenticate_password(ctx,
                             (uint8_t *) password, strlen(password));
    ASSERT_EQ(rc, -PB_RESULT_AUTHENTICATION_FAILED);


    rc = pb_api_auth_set_otp_password(ctx, password, strlen(password));
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(api_auth_set_otp_pass_again)
{
    int rc;
    struct pb_context *ctx;
    const char password[] = "0123456789abcdef";

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Create command context and connect */
    rc = pb_api_create_context(&ctx, pb_test_debug);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Authenticate, should fail since no password is set yet  */
    rc = pb_api_authenticate_password(ctx,
                             (uint8_t *) password, strlen(password));
    ASSERT_EQ(rc, -PB_RESULT_AUTHENTICATION_FAILED);


    rc = pb_api_auth_set_otp_password(ctx, password, strlen(password));
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_api_auth_set_otp_password(ctx, password, strlen(password));
    ASSERT_EQ(rc, -PB_RESULT_ERROR);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(api_auth_ok)
{
    int rc;
    struct pb_context *ctx;
    const char password[] = "0123456789abcdef";

    /* Start command loop */
    rc = test_command_loop_start(NULL);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Create command context and connect */
    rc = pb_api_create_context(&ctx, pb_test_debug);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_socket_transport_init(ctx, "/tmp/pb_test_sock");
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = ctx->connect(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_api_auth_set_otp_password(ctx, password, strlen(password));
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Authenticate */
    rc = pb_api_authenticate_password(ctx,
                             (uint8_t *) password, strlen(password));
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Send device reset command */
    rc = pb_api_device_reset(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}
