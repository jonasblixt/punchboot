#ifndef TEST_COMMAND_LOOP_H_
#define TEST_COMMAND_LOOP_H_

#include <pb-tools/error.h>
#include "command.h"

int test_command_loop_start(struct pb_command_ctx **ctxp);
int test_command_loop_stop(void);
void test_command_loop_set_authenticated(bool authenticated);

#endif  // TEST_COMMAND_LOOP_H_
