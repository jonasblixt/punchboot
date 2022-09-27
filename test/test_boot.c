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

TEST(boot)
{
    int rc;
    struct pb_context *ctx;

    uint8_t partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
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

    /* Boot */
    rc = pb_api_boot_part(ctx, partuuid, false);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(boot_non_bootable)
{
    int rc;
    struct pb_context *ctx;

    uint8_t partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
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

    /* Boot */
    rc = pb_api_boot_part(ctx, partuuid, false);
    ASSERT_EQ(rc, -PB_RESULT_PART_NOT_BOOTABLE);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(boot_invalid_uuid)
{
    int rc;
    struct pb_context *ctx;

    uint8_t partuuid[] = "\xFF\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
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

    /* Boot */
    rc = pb_api_boot_part(ctx, partuuid, false);
    ASSERT_EQ(rc, -PB_RESULT_ERROR);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}


TEST(boot_ram)
{
    int rc;
    struct pb_context *ctx;
    uint8_t *data = malloc(1024*1024);

    printf("Reading bpak file...\n");

    ASSERT(system(TEST_SRC_DIR "/prepare_bpak_file.sh") == 0);


    FILE *fp = fopen("test.bpak", "rb");

    ASSERT(fp != NULL)

    fread(data, 1, 1024*1024, fp);
    fclose(fp);
    printf("Done\n");

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

    /* Boot */
    rc = pb_api_boot_ram(ctx, data, NULL, false);
    ASSERT_EQ(rc, PB_RESULT_OK);


    free(data);

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
    uint8_t partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
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
    rc = pb_api_boot_activate(ctx, partuuid);
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
    uint8_t partuuid[] = "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62" \
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
    rc = pb_api_boot_activate(ctx, partuuid);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}
