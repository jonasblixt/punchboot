#ifndef TEST_COMMON_H_
#define TEST_COMMON_H_

#include <pb-tools/api.h>

int pb_test_debug(struct pb_context *ctx, int level,
                                const char *fmt, ...);

#endif  // TEST_COMMON_H_
