#ifndef TOOL_H_
#define TOOL_H_

#include <stdio.h>

#include <pb/api.h>
#include <pb/error.h>

void print_version(void);
int pb_get_verbosity(void);
void pb_inc_verbosity(void);

void help_main(void);
void help_dev(void);

int action_dev(int argc, char **argv);

int transport_init_helper(struct pb_context **ctxp, const char *transport_name);

#endif  // TOOL_H_
