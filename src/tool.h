#ifndef TOOL_H_
#define TOOL_H_

#include <stdio.h>

#include <pb-tools/api.h>
#include <pb-tools/error.h>

void print_version(void);
int pb_get_verbosity(void);
void pb_inc_verbosity(void);

void help_main(void);
void help_dev(void);
void help_part(void);
void help_boot(void);
void help_auth(void);
void help_slc(void);

int action_dev(int argc, char **argv);
int action_part(int argc, char **argv);
int action_boot(int argc, char **argv);

int transport_init_helper(struct pb_context **ctxp, const char *transport_name);
int pb_debug(struct pb_context *ctx, int level, const char *fmt, ...);
int bytes_to_string(size_t bytes, char *out, size_t size);

#endif  // TOOL_H_
