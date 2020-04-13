#include <string.h>
#include "nala.h"
#include <uuid.h>
#include <pb-tools/api.h>
#include <pb-tools/wire.h>
#include <pb-tools/error.h>
#include <pb-tools/socket.h>

#include "test_command_loop.h"
#include "command.h"
#include "common.h"

TEST(api_slc1)
{
    int rc;
    struct pb_context *ctx;
    uint8_t slc = PB_SLC_INVALID;
    uint32_t active_keys[16];
    uint32_t revoked_keys[16];

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


    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                            (uint8_t *) active_keys,
                            (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_NOT_CONFIGURED);

    printf("slc: %s\n", pb_wire_slc_string(slc));

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                            (uint8_t *) active_keys,
                            (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_NOT_CONFIGURED);

    printf("slc: %s\n", pb_wire_slc_string(slc));

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(api_slc_configuration)
{
    int rc;
    struct pb_context *ctx;
    uint8_t slc = PB_SLC_INVALID;

    uint32_t active_keys[16];
    uint32_t revoked_keys[16];
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


    /* Perform device configuration */
    rc = pb_api_slc_set_configuration(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);
    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION);

    /* Additional calls to device_configure is allowed */

    /* Perform device configuration */
    rc = pb_api_slc_set_configuration(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION);

    printf("slc: %s\n", pb_wire_slc_string(slc));
    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}


TEST(api_slc_configuration_lock)
{
    int rc;
    struct pb_context *ctx;

    uint8_t slc = PB_SLC_INVALID;
    uint32_t active_keys[16];
    uint32_t revoked_keys[16];
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

    /* Perform device configuration */
    rc = pb_api_slc_set_configuration(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);
    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION);

    /* Perform device configuration */
    rc = pb_api_slc_set_configuration_lock(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION_LOCKED);

    /* Additional calls to device_configuration_lock is _not_ allowed */

    /* Perform device configuration */
    rc = pb_api_slc_set_configuration_lock(ctx);
    ASSERT_EQ(rc, -PB_RESULT_ERROR);

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION_LOCKED);
    printf("slc: %s\n", pb_wire_slc_string(slc));

    /* Perform device configuration */
    rc = pb_api_slc_set_configuration(ctx);
    ASSERT_EQ(rc, -PB_RESULT_ERROR);

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_CONFIGURATION_LOCKED);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(api_slc_revoke_key)
{
    int rc;
    struct pb_context *ctx;
    uint8_t slc = PB_SLC_INVALID;
    uint32_t active_keys[16];
    uint32_t revoked_keys[16];

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

    memset(active_keys, 0, sizeof(active_keys));
    memset(revoked_keys, 0, sizeof(revoked_keys));

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_NOT_CONFIGURED);

    printf("slc: %s\n", pb_wire_slc_string(slc));

    ASSERT_EQ(active_keys[0], 0x02020202);
    ASSERT_EQ(active_keys[1], 0x03030303);
    ASSERT_EQ(active_keys[2], 0x04040404);
    ASSERT_EQ(active_keys[3], 0x00000000);

    ASSERT_EQ(revoked_keys[0], 0x01010101);
    ASSERT_EQ(revoked_keys[1], 0x00000000);


    /* Revoke key 0x02020202 */

    rc = pb_api_slc_revoke_key(ctx, 0x02020202);
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_NOT_CONFIGURED);

    printf("slc: %s\n", pb_wire_slc_string(slc));

    ASSERT_EQ(active_keys[0], 0x00000000);
    ASSERT_EQ(active_keys[1], 0x03030303);
    ASSERT_EQ(active_keys[2], 0x04040404);
    ASSERT_EQ(active_keys[3], 0x00000000);

    ASSERT_EQ(revoked_keys[0], 0x01010101);
    ASSERT_EQ(revoked_keys[1], 0x02020202);
    ASSERT_EQ(revoked_keys[2], 0x00000000);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

TEST(api_slc_revoke_key_twice)
{
    int rc;
    struct pb_context *ctx;
    uint8_t slc = PB_SLC_INVALID;
    uint32_t active_keys[16];
    uint32_t revoked_keys[16];

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

    memset(active_keys, 0, sizeof(active_keys));
    memset(revoked_keys, 0, sizeof(revoked_keys));

    /* Read security life cycle */
    rc = pb_api_slc_read(ctx, &slc,
                                (uint8_t *) active_keys,
                                (uint8_t *) revoked_keys);

    ASSERT_EQ(rc, PB_RESULT_OK);
    ASSERT_EQ(slc, PB_SLC_NOT_CONFIGURED);

    printf("slc: %s\n", pb_wire_slc_string(slc));

    ASSERT_EQ(active_keys[0], 0x02020202);
    ASSERT_EQ(active_keys[1], 0x03030303);
    ASSERT_EQ(active_keys[2], 0x04040404);
    ASSERT_EQ(active_keys[3], 0x00000000);

    ASSERT_EQ(revoked_keys[0], 0x01010101);
    ASSERT_EQ(revoked_keys[1], 0x00000000);


    /* Revoke key 0x02020202 */

    rc = pb_api_slc_revoke_key(ctx, 0x02020202);
    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_api_slc_revoke_key(ctx, 0x02020202);
    ASSERT_EQ(rc, -PB_RESULT_ERROR);

    /* Stop command loop */
    rc = test_command_loop_stop();
    ASSERT_EQ(rc, PB_RESULT_OK);

    /* Free command context */
    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}

