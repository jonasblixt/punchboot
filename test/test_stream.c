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

TEST(stream_init)
{
    int rc;
    struct pb_context *ctx;

    char partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe1";

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

    /* Initialize stream */
    rc = pb_api_stream_init(ctx, partuuid);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}


TEST(stream_write)
{
    int rc;
    struct pb_context *ctx;
    uint8_t chunk[4096];
    char partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe1";

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

    /* Initialize stream */
    rc = pb_api_stream_init(ctx, partuuid);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Write loop */
    uint8_t chunks_to_write = 10;
    uint8_t buffer_id = 0;
    uint64_t offset = 0;

    while(chunks_to_write)
    {
        rc = pb_api_stream_prepare_buffer(ctx, buffer_id, chunk, sizeof(chunk));
        ASSERT_EQ(rc, PB_RESULT_OK);

        rc = pb_api_stream_write_buffer(ctx, buffer_id, offset, sizeof(chunk));
        ASSERT_EQ(rc, PB_RESULT_OK);

        offset += sizeof(chunk);
        chunks_to_write--;
        buffer_id = !buffer_id;
    }

    rc = pb_api_stream_finalize(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(stream_unaligned_offset)
{
}

TEST(stream_out_of_bounds)
{
}

