#include <stdio.h>
#include <pb-tools/api.h>
#include <pb-tools/error.h>

#include "nala.h"
#include "common.h"

TEST(alloc_free_ctx)
{
    struct pb_context *ctx = NULL;
    int rc;

    rc = pb_api_create_context(&ctx, pb_test_debug);

    ASSERT_EQ(rc, PB_RESULT_OK);

    rc = pb_api_free_context(ctx);
    ASSERT_EQ(rc, PB_RESULT_OK);
}
