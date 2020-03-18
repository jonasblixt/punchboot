#include <string.h>
#include "nala.h"
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>
#include <pb-tools/slc.h>

#include "test_command_loop.h"
#include "command.h"

TEST(api_slc1)
{
    int rc;
    struct pb_context *ctx;
    uint32_t slc = 0;

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


    /* Read security life cycle */
    rc = pb_api_device_read_slc(ctx, &slc);
    ASSERT_EQ(rc, PB_OK);
    ASSERT_EQ(slc, PB_SLC_NOT_CONFIGURED);

    printf("slc: %s\n", pb_slc_string(slc));
    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}

TEST(api_slc_configuration)
{
    int rc;
    struct pb_context *ctx;
    uint32_t slc = 0;

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


    /* Perform device configuration */
    rc = pb_api_device_configure(ctx, NULL, 0);
    ASSERT_EQ(rc, PB_OK);

    /* Read security life cycle */
    rc = pb_api_device_read_slc(ctx, &slc);
    ASSERT_EQ(rc, PB_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION);

    /* Additional calls to device_configure is allowed */

    /* Perform device configuration */
    rc = pb_api_device_configure(ctx, NULL, 0);
    ASSERT_EQ(rc, PB_OK);

    /* Read security life cycle */
    rc = pb_api_device_read_slc(ctx, &slc);
    ASSERT_EQ(rc, PB_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION);

    printf("slc: %s\n", pb_slc_string(slc));
    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}


TEST(api_slc_configuration_lock)
{
    int rc;
    struct pb_context *ctx;
    uint32_t slc = 0;

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

    /* Perform device configuration */
    rc = pb_api_device_configure(ctx, NULL, 0);
    ASSERT_EQ(rc, PB_OK);

    /* Read security life cycle */
    rc = pb_api_device_read_slc(ctx, &slc);
    ASSERT_EQ(rc, PB_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION);

    /* Perform device configuration */
    rc = pb_api_device_configuration_lock(ctx, NULL, 0);
    ASSERT_EQ(rc, PB_OK);

    /* Read security life cycle */
    rc = pb_api_device_read_slc(ctx, &slc);
    ASSERT_EQ(rc, PB_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION_LOCKED);

    /* Additional calls to device_configuration_lock is _not_ allowed */

    /* Perform device configuration */
    rc = pb_api_device_configuration_lock(ctx, NULL, 0);
    ASSERT_EQ(rc, -PB_ERR);

    /* Read security life cycle */
    rc = pb_api_device_read_slc(ctx, &slc);
    ASSERT_EQ(rc, PB_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION_LOCKED);
    printf("slc: %s\n", pb_slc_string(slc));

    /* Perform device configuration */
    rc = pb_api_device_configure(ctx, NULL, 0);
    ASSERT_EQ(rc, -PB_ERR);

    /* Read security life cycle */
    rc = pb_api_device_read_slc(ctx, &slc);
    ASSERT_EQ(rc, PB_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION_LOCKED);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}
