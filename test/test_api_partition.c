#include <string.h>
#include "nala.h"
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>
#include <pb-tools/partition.h>

#include "test_command_loop.h"
#include "command.h"

TEST(api_partition)
{
    int rc;
    struct pb_context *ctx;
    struct pb_partition_table_entry tbl[32];

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

    test_command_loop_set_authenticated(true);

    /* Read device partition table*/
    //rc = pb_api_partition_read(ctx, tbl, sizeof(tbl));
    //ASSERT_EQ(rc, PB_OK);


    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}
