/**
 * \file command.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Named commands: functions of pb's that a host calls by name
 * (doc/architecture/command-mode.md 5.6, D-076).
 *
 * The core, a platform, a driver and the board each declare their own with
 * DEFINE_COMMAND(). The host runs one with BOARD_COMMAND, by pb_id() of its
 * name, and lists them with COMMAND_LIST. Command mode runs a command only
 * after its gate, and never on the boot path.
 *
 * What a command's function may rely on, and what it does:
 *
 *  - The request is the host's and untrusted in every state: at most 1024
 *    bytes, exactly 'request_size' of them. Every size and offset read in it
 *    is checked against 'request_size'.
 *  - '*response_size' holds the response's room on the call. The function
 *    sets it to what it wrote, also when it fails.
 *  - It returns PB_OK or -PB_ERR_*, which the host gets.
 *  - A command that runs long calls plat_wdog_kick() as it makes progress.
 *  - It may ask for a reset with cm_request_reset_on_success() (pb/cm.h).
 *  - It stays inside the core's rules: it starts no image, writes no fuse and
 *    changes neither the keys nor the life cycle.
 *
 * Once released, a command's name and what its bytes mean are fixed.
 */

#ifndef INCLUDE_PB_COMMAND_H
#define INCLUDE_PB_COMMAND_H

#include <pb/pb.h>
#include <stddef.h>
#include <stdint.h>

/* A name is 1 to 31 characters of [a-z0-9-], a help line at most 79
 * printable ones: each fits a list entry's field with its NUL */
#define CM_COMMAND_NAME_MAX 31
#define CM_COMMAND_HELP_MAX 79

typedef int (*cm_command_fn_t)(const uint8_t *request,
                               size_t request_size,
                               uint8_t *response,
                               size_t *response_size);

struct cm_command {
    const char *name;
    const char *help;
    cm_command_fn_t run;
};

/**
 * Declare a named command
 *
 * The command is put into a list that the linker builds, in the section
 * 'pb_commands', which every link script keeps
 * (doc/interfaces/platform.md 5.1). Command mode checks the list when it is
 * set up: a bad list means no command mode.
 *
 * @param[in] cmd_name The name, a string of 1 to 31 characters of [a-z0-9-],
 *                     not "list". Its id is pb_id() of it
 * @param[in] cmd_help One line for the host's list, at most 79 characters
 * @param[in] run_fn   The function, a cm_command_fn_t. It names the entry,
 *                     so one function is one command
 */
#define DEFINE_COMMAND(cmd_name, cmd_help, run_fn)                                     \
    static const struct cm_command pb_command_##run_fn __used __section("pb_commands") \
        __aligned(__alignof__(                                                         \
            struct cm_command)) = { .name = (cmd_name), .help = (cmd_help), .run = (run_fn) }

/* The list's start and end: the link script's on a target, the linker's own
 * for the host. Weak, so that a program without a command links with an
 * empty list. They are compared with '<' only */
extern const struct cm_command __start_pb_commands[] __attribute__((weak));
extern const struct cm_command __stop_pb_commands[] __attribute__((weak));

/**
 * Check a list of commands (command-mode.md 2.1, 5.6): every name is 1 to
 * 31 characters of [a-z0-9-] and not "list", no two names have one id, every
 * help line is at most 79 printable characters, every command has a
 * function, and the list's entries fit in 'room' bytes.
 *
 * @param[in] list  The first command
 * @param[in] count The number of commands
 * @param[in] room  The bytes COMMAND_LIST has for its entries
 *
 * @return PB_OK, or -PB_ERR_PARAM for a list that isn't valid
 */
int cm_commands_check(const struct cm_command *list, size_t count, size_t room);

/**
 * The build's command of an id
 *
 * @param[in] id pb_id() of the command's name
 *
 * @return The command, or NULL when the build has none of that id
 */
const struct cm_command *cm_command_find(uint32_t id);

/**
 * The build's commands, from the linker's list
 *
 * @param[out] count The number of commands
 *
 * @return The first command
 */
const struct cm_command *cm_commands(size_t *count);

#endif // INCLUDE_PB_COMMAND_H
