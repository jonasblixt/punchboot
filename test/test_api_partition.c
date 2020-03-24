#include <string.h>
#include "nala.h"
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>
#include <pb-tools/wire.h>

#include "test_command_loop.h"
#include "command.h"
#include "common.h"

TEST(api_partition)
{
    int rc;
    struct pb_context *ctx;
    struct pb_partition_table_entry tbl[128];
    int entries = 128;

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

    test_command_loop_set_authenticated(true);

    /* Read device partition table*/
    rc = pb_api_partition_read_table(ctx, tbl, &entries);
    ASSERT_EQ(rc, PB_RESULT_OK);


    for (int i = 0; i < entries; i++)
    {
        ssize_t bytes = (tbl[i].last_block - tbl[i].first_block) * \
                            tbl[i].block_size;
        printf("%s, %li bytes\n", tbl[i].description, bytes);
    }


    /* Install partition table*/
    rc = pb_api_partition_install_table(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    entries = 16;

    /* Read device partition table*/
    rc = pb_api_partition_read_table(ctx, tbl, &entries);
    ASSERT_EQ(rc, PB_RESULT_OK);

    for (int i = 0; i < entries; i++)
    {
        ssize_t bytes = (tbl[i].last_block - tbl[i].first_block) * \
                            tbl[i].block_size;
        printf("%s, %li bytes\n", tbl[i].description, bytes);
    }
    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}


TEST(part_verify)
{
    int rc;
    struct pb_context *ctx;
    struct pb_result_part_table_entry tbl[128];
    char partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe0";
    char sha256[] = "hejhopp123";

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

    /* Install partition table*/
    rc = pb_api_partition_install_table(ctx);

    ASSERT_EQ(rc, PB_RESULT_OK);
    /* Verify partition */
    rc = pb_api_partition_verify(ctx, partuuid, sha256, 16, false);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}


TEST(part_verify_fail)
{
    int rc;
    struct pb_context *ctx;
    struct pb_result_part_table_entry tbl[128];
    char partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe0";
    char sha256[] = "hejhopp124";

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

    /* Install partition table*/
    rc = pb_api_partition_install_table(ctx);

    ASSERT_EQ(rc, PB_RESULT_OK);
    /* Verify partition */
    rc = pb_api_partition_verify(ctx, partuuid, sha256, 16, false);
    ASSERT_EQ(rc, -PB_RESULT_PART_VERIFY_FAILED);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(part_verify_invalid_part)
{
    int rc;
    struct pb_context *ctx;
    struct pb_result_part_table_entry tbl[128];
    char partuuid[] = "\xff\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe0";
    char sha256[] = "hejhopp123";

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

    /* Install partition table*/
    rc = pb_api_partition_install_table(ctx);

    ASSERT_EQ(rc, PB_RESULT_OK);
    /* Verify partition */
    rc = pb_api_partition_verify(ctx, partuuid, sha256, 16, false);
    ASSERT_EQ(rc, -PB_RESULT_INVALID_ARGUMENT);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(activate_part_not_bootable)
{
    int rc;
    struct pb_context *ctx;
    struct pb_result_part_table_entry tbl[128];
    char partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
                                "\x01\x4c\xfa\x73\x5c\xe0";

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

    /* Install partition table*/
    rc = pb_api_partition_install_table(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Activate partition */
    rc = pb_api_partition_activate(ctx, partuuid);
    ASSERT_EQ(rc, -PB_RESULT_PART_NOT_BOOTABLE);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}


TEST(activate_part)
{
    int rc;
    struct pb_context *ctx;
    struct pb_result_part_table_entry tbl[128];
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

    /* Install partition table*/
    rc = pb_api_partition_install_table(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Activate partition */
    rc = pb_api_partition_activate(ctx, partuuid);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}


TEST(read_bpak)
{
    int rc;
    struct pb_context *ctx;
    struct bpak_header b;
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

    /* Install partition table*/
    rc = pb_api_partition_install_table(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Try to read bpak header from partition */
    rc = pb_api_partition_read_bpak(ctx, partuuid, &b);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = bpak_valid_header(&b);
    ASSERT_EQ(rc, BPAK_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

