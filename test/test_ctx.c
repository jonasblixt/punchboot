#include <stdio.h>
#include <pb/api.h>
#include <pb/error.h>
#include "nala.h"

TEST(alloc_free_ctx)
{
    struct pb_context *ctx = NULL;
    int rc;

    rc = pb_create_context(&ctx);

    ASSERT_EQ(rc, PB_OK);

    rc = pb_free_context(ctx);
    ASSERT_EQ(rc, PB_OK);
}
