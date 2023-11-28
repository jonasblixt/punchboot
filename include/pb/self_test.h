/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_SELF_TEST_H
#define INCLUDE_SELF_TEST_H

#include <pb/utils_def.h>

struct self_test {
    const char *name;
    int (*test)(void);
};

#define DECLARE_SELF_TEST(tst_name)                                                          \
    static int tst_name(void);                                                               \
    static const struct self_test self_test_##tst_name __used __section(".self_test_fn") = { \
        .name = #tst_name,                                                                   \
        .test = tst_name,                                                                    \
    };                                                                                       \
    static int tst_name(void)

void self_test(void);

#endif // INCLUDE_SELF_TEST_H
