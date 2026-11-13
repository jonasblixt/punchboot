/**
 * \file task.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Tasks: what runs while pb is stopped (doc/interfaces/board.md 4.5, D-051).
 *
 * When the boot failed, was refused or was stopped, main() calls every task
 * of the build, one after the other, again and again, until one returns an
 * error. Then pb resets. Command mode's handling of the host is one task
 * (cm_main_task), in a build that has it. Anything that is linked can define
 * one: a board, a platform, a driver.
 *
 * What a task may rely on:
 *
 *  - No switching and no pre-emption: a task runs until it returns.
 *  - No order: which task runs first is not defined.
 *  - Never on the boot path: only when pb is stopped.
 *  - No set up function: a task that needs one does it on its first call.
 *  - The core kicks the watchdog, once per round.
 *
 * What a task does: it returns quickly and never waits for hardware, takes
 * no input from outside the device (only command mode's task reads the
 * transport), and stays inside the core's rules: it starts no image, writes
 * no fuse and changes neither the keys nor the life cycle.
 */

#ifndef INCLUDE_PB_TASK_H
#define INCLUDE_PB_TASK_H

#include <pb/pb.h>

struct pb_task {
    const char *name;
    /* PB_OK to be called again. Any error ends the loop, and pb resets */
    int (*run)(void);
};

/**
 * Define a task
 *
 * The task is put into a list that the linker builds, in the section
 * 'pb_tasks', which every link script keeps (doc/interfaces/platform.md 5.1).
 *
 * @param[in] task_name The task's name, an identifier: it is in pb's log
 *                      when the task ends the loop
 * @param[in] run_fn int (*)(void): PB_OK, or -PB_ERR_* to end the loop
 */
#define DEFINE_TASK(task_name, run_fn)                                           \
    static const struct pb_task pb_task_##task_name __used __section("pb_tasks") \
        __aligned(__alignof__(struct pb_task)) = { .name = #task_name, .run = (run_fn) }

/* The list's start and end: the link script's on a target, the linker's own
 * for the host. They are compared with '<' only: two symbols that the
 * compiler takes for different objects never compare equal */
extern const struct pb_task __start_pb_tasks[];
extern const struct pb_task __stop_pb_tasks[];

#endif // INCLUDE_PB_TASK_H
