#include <stdio.h>
#include <string.h>
#include <pb-tools/error.h>
#include "nala.h"

TEST(error_codes)
{
    for (int i = 0; i < PB_RESULT_END; i++)
    {
        printf("Testing error code: %i\n", -i);

        const char *str = pb_error_string(-i);

        ASSERT(strlen(str) > 0);

        printf("Error code %i (%s)\n", -i, str);
    }
}
