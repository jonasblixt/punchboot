#include <stdio.h>
#include <string.h>
#include <pb-tools/api.h>
#include <pb-tools/protocol.h>
#include <pb-tools/usb.h>
#include "tool.h"

int transport_init_helper(struct pb_context **ctxp, const char *transport_name)
{
    int rc;
    char *t = (char *) transport_name;

    if (!t)
        t = (char *) "usb";

    if (pb_get_verbosity() > 2)
        printf("Initializing transport: %s\n", t);

    struct pb_context *ctx = NULL;
    rc = pb_api_create_context(&ctx);

    if (rc != PB_OK)
        return rc;
    *ctxp = ctx;

    if (strcmp(t, "usb") == 0)
    {
        rc = pb_usb_transport_init(ctx);
    }
    else if (strcmp(t, "socket") == 0)
    {
    }

    if (rc != PB_OK)
        printf("Error: Could not initialize transport\n");

    return rc;
}
