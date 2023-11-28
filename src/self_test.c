/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <inttypes.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/self_test.h>
#include <pb/timestamp.h>

IMPORT_SYM(const struct self_test *, _self_test_fn_start, self_test_fn_start);
IMPORT_SYM(const struct self_test *, _self_test_fn_end, self_test_fn_end);

void self_test(void)
{
    const struct self_test *test;
    int rc;
    int fail_count = 0;
    int pass_count = 0;

    printf("Running built in self-tests:\n\r");
    ts("self test begin");
    for (test = self_test_fn_start; test != self_test_fn_end; test++) {
        plat_wdog_kick();
        rc = test->test();
        if (rc == 0) {
            pass_count++;
            printf("%s - OK\n\r", test->name);
        } else {
            fail_count++;
            printf("%s - Failed (%i)\n\r", test->name, rc);
        }
    }

    ts("self test end");

    if (fail_count == 0)
        LOG_INFO("All %i tests passed", pass_count);
    else
        LOG_INFO("%i Pass, %i Failed", pass_count, fail_count);
}
