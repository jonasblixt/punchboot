#include <string.h>
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>

#include "nala.h"
#include "test_command_loop.h"
#include "command.h"
#include "common.h"

TEST(board_command)
{
    int rc;
    struct pb_context *ctx;

    char request[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe1";

    char response_buffer[128];

    test_command_loop_set_authenticated(true);

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

    memset(response_buffer, 0, strlen(response_buffer));
    /* Call board command  */
    rc = pb_api_board_command(ctx, 1, request, strlen(request),
                                      response_buffer, sizeof(response_buffer));
    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT(strcmp(response_buffer, "Hello") == 0);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(board_status)
{
    int rc;
    struct pb_context *ctx;
    char response_buffer[128];

    test_command_loop_set_authenticated(true);

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

    memset(response_buffer, 0, sizeof(response_buffer));
    /* Call board command  */
    rc = pb_api_board_status(ctx, response_buffer, sizeof(response_buffer));
    ASSERT_EQ(rc, PB_RESULT_OK);
    printf("Status: %s\n", response_buffer);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

