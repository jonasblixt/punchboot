/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * main() (src/main.c) and its loop over the tasks (board.md 4.5, system.md
 * 2): when the boot failed or was stopped, every task that DEFINE_TASK()
 * defines is called in turn, again and again, until one returns an error,
 * and then pb resets. The platform, the board's set up and the boot are
 * fakes; plat_reset() and a boot that jumps leave main() with a longjmp.
 *
 * Built twice: with three tasks, and with TEST_NO_TASKS for a build that has
 * none, where the list's bounds come from the link (CMakeLists.txt).
 *
 * And what main() does after a platform failure (platform.md 2): it resets,
 * and neither the boot nor a task runs.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <boot/boot.h>
#include <pb/arch.h>
#include <pb/board.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/ramlog.h>
#include <pb/task.h>
#include <string.h>

#include "fakes/plat.h"

#define LEFT_BY_RESET 1
#define LEFT_BY_JUMP  2
#define N_TASKS       3

static jmp_buf leave;

static struct {
    int early_init_rc;
    int init_rc;
    int early_init_calls;
    int console_init_calls;
    int init_calls;
    int setup_rc;
    int boot_rc;
    bool boot_jumps;
    int setup_calls;
    int boot_calls;
    int resets;
    /* The tasks: 'a', 'b' and 'c' */
    int calls[N_TASKS];
    int fail_at[N_TASKS]; /* The call that returns fail_rc, from 1; 0: never */
    int fail_rc[N_TASKS];
    char order[64]; /* Every call, in order */
    size_t n_order;
    unsigned int kicks_at[64]; /* fake_wdog_kicks at each call */
} f;

void arch_init(void)
{
}

void ramlog_init(void)
{
}

int plat_early_init(void)
{
    f.early_init_calls++;
    return f.early_init_rc;
}

int plat_init(void)
{
    f.init_calls++;
    return f.init_rc;
}

void plat_reset(void)
{
    f.resets++;
    longjmp(leave, LEFT_BY_RESET);
}

static void console_init(void)
{
    f.console_init_calls++;
}

const struct board board = {
    .name = "test",
    .console_init = console_init,
};

int board_setup(void)
{
    f.setup_calls++;
    return f.setup_rc;
}

int boot(uuid_t boot_part_override_uu)
{
    (void)boot_part_override_uu;
    f.boot_calls++;

    /* The next stage runs: nothing of main() after it does */
    if (f.boot_jumps)
        longjmp(leave, LEFT_BY_JUMP);

    return f.boot_rc;
}

#ifndef TEST_NO_TASKS
static int task(int id)
{
    f.calls[id]++;
    assert_true(f.n_order < sizeof(f.order) - 1);
    f.kicks_at[f.n_order] = fake_wdog_kicks;
    f.order[f.n_order++] = (char)('a' + id);

    if (f.calls[id] == f.fail_at[id])
        return f.fail_rc[id];

    return PB_OK;
}

static int task_a(void)
{
    return task(0);
}

static int task_b(void)
{
    return task(1);
}

static int task_c(void)
{
    return task(2);
}

DEFINE_TASK(test_a, task_a);
DEFINE_TASK(test_b, task_b);
DEFINE_TASK(test_c, task_c);
#endif

/* Runs main() and returns how it was left */
static int run_main(void)
{
    switch (setjmp(leave)) {
    case 0:
        pb_main();
        return 0; /* Not reached: main() ends in plat_reset() */
    case LEFT_BY_JUMP:
        return LEFT_BY_JUMP;
    default:
        return LEFT_BY_RESET;
    }
}

static int setup(void **state)
{
    (void)state;
    memset(&f, 0, sizeof(f));
    f.boot_rc = -PB_ERR;
    return 0;
}

/* Nothing ran after the platform's failure but the reset */
static void assert_reset_only(void)
{
    assert_int_equal(f.setup_calls, 0);
    assert_int_equal(f.boot_calls, 0);
    assert_int_equal(f.n_order, 0);
    for (int id = 0; id < N_TASKS; id++)
        assert_int_equal(f.calls[id], 0);
    assert_int_equal(f.resets, 1);
}

/* platform.md 2, system.md 5.4: a failed plat_early_init() resets. pb isn't
 * stopped: no task runs (command mode is one), and nothing of the boot */
static void test_plat_early_init_fails_resets(void **state)
{
    (void)state;

    f.early_init_rc = -PB_ERR_IO;
    assert_int_equal(run_main(), LEFT_BY_RESET);
    assert_int_equal(f.early_init_calls, 1);
    assert_int_equal(f.console_init_calls, 0);
    assert_int_equal(f.init_calls, 0);
    assert_reset_only();
}

/* platform.md 2, 2.2: a failed plat_init() resets, after the console was
 * set up: what command mode needs is missing (F-202: the i.MX8M's DRAM) */
static void test_plat_init_fails_resets(void **state)
{
    (void)state;

    f.init_rc = -PB_ERR_MEM;
    assert_int_equal(run_main(), LEFT_BY_RESET);
    assert_int_equal(f.early_init_calls, 1);
    assert_int_equal(f.console_init_calls, 1);
    assert_int_equal(f.init_calls, 1);
    assert_reset_only();
}

/* platform.md 2: the order, when nothing fails */
static void test_platform_is_set_up_before_the_board(void **state)
{
    (void)state;

    f.boot_jumps = true;
    assert_int_equal(run_main(), LEFT_BY_JUMP);
    assert_int_equal(f.early_init_calls, 1);
    assert_int_equal(f.console_init_calls, 1);
    assert_int_equal(f.init_calls, 1);
    assert_int_equal(f.setup_calls, 1);
    assert_int_equal(f.boot_calls, 1);
}

#ifndef TEST_NO_TASKS
/* board.md 4.5: the core calls every task in the list, one after the other,
 * again and again. An error ends the loop, also for the tasks that would
 * have followed in that round, and pb resets (system.md 2) */
static void test_tasks_run_in_turn_until_one_ends_the_loop(void **state)
{
    (void)state;

    f.fail_at[1] = 3;
    f.fail_rc[1] = -PB_ERR_ABORT;
    assert_int_equal(run_main(), LEFT_BY_RESET);
    assert_int_equal(f.resets, 1);
    assert_int_equal(f.calls[1], 3);

    /* Two whole rounds: each task once in each, in the same order (which
     * order is not defined) */
    assert_true(f.n_order >= 2 * N_TASKS + 1);
    assert_memory_equal(&f.order[0], &f.order[N_TASKS], N_TASKS);
    for (int id = 0; id < N_TASKS; id++)
        assert_non_null(memchr(f.order, 'a' + id, N_TASKS));

    /* The third round ends with the task that failed, and nothing is called
     * after it */
    assert_int_equal(f.order[f.n_order - 1], 'b');
    assert_memory_equal(&f.order[0], &f.order[2 * N_TASKS], f.n_order - 2 * N_TASKS);
    assert_true(f.n_order <= 3 * N_TASKS);
}

/* board.md 4.5: PB_OK is "call me again", any error ends the loop */
static void test_any_error_ends_the_loop(void **state)
{
    static const int errors[] = { -PB_ERR, -PB_ERR_ABORT, -PB_ERR_TIMEOUT, -PB_ERR_IO };
    (void)state;

    for (size_t i = 0; i < ARRAY_SIZE(errors); i++) {
        for (int id = 0; id < N_TASKS; id++) {
            setup(NULL);
            f.fail_at[id] = 1;
            f.fail_rc[id] = errors[i];
            assert_int_equal(run_main(), LEFT_BY_RESET);
            assert_int_equal(f.resets, 1);
            assert_int_equal(f.calls[id], 1);
            /* The first round was the last */
            assert_true(f.n_order <= N_TASKS);
            assert_int_equal(f.order[f.n_order - 1], 'a' + id);
        }
    }
}

/* board.md 4.5, platform.md 3.3: a task doesn't kick the watchdog, the core
 * does, once per round */
static void test_watchdog_is_kicked_once_per_round(void **state)
{
    unsigned int kicks = fake_wdog_kicks;
    (void)state;

    f.fail_at[0] = 5;
    f.fail_rc[0] = -PB_ERR;
    assert_int_equal(run_main(), LEFT_BY_RESET);
    assert_int_equal(f.calls[0], 5);
    assert_int_equal(fake_wdog_kicks - kicks, 5);

    /* Before the round's first task, and not between its tasks */
    for (size_t i = 0; i < f.n_order; i++)
        assert_int_equal(f.kicks_at[i] - kicks, i / N_TASKS + 1);
}

/* board.md 4.5: never on the boot path (D-023). No task runs before the
 * boot, and none when the boot jumps */
static void test_tasks_run_only_when_stopped(void **state)
{
    (void)state;

    f.boot_jumps = true;
    assert_int_equal(run_main(), LEFT_BY_JUMP);
    assert_int_equal(f.setup_calls, 1);
    assert_int_equal(f.boot_calls, 1);
    assert_int_equal(f.n_order, 0);
    assert_int_equal(f.resets, 0);
}

/* board.md 2.1, 4: pb is stopped when board_setup() fails, as when the boot
 * does: the boot isn't tried, and the tasks run */
static void test_stopped_after_a_failed_board_setup(void **state)
{
    (void)state;

    f.setup_rc = -PB_ERR_PARAM;
    f.fail_at[2] = 2;
    f.fail_rc[2] = -PB_ERR;
    assert_int_equal(run_main(), LEFT_BY_RESET);
    assert_int_equal(f.boot_calls, 0);
    assert_int_equal(f.calls[2], 2);
    assert_int_equal(f.resets, 1);
}
#else
/* board.md 4.5, system.md 2: no task in the build, as one without command
 * mode may have: pb resets at once */
static void test_no_task_resets_at_once(void **state)
{
    (void)state;

    assert_int_equal(run_main(), LEFT_BY_RESET);
    assert_int_equal(f.boot_calls, 1);
    assert_int_equal(f.resets, 1);

    setup(NULL);
    f.setup_rc = -PB_ERR_PARAM;
    assert_int_equal(run_main(), LEFT_BY_RESET);
    assert_int_equal(f.boot_calls, 0);
    assert_int_equal(f.resets, 1);
}
#endif

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_plat_early_init_fails_resets, setup),
        cmocka_unit_test_setup(test_plat_init_fails_resets, setup),
        cmocka_unit_test_setup(test_platform_is_set_up_before_the_board, setup),
#ifndef TEST_NO_TASKS
        cmocka_unit_test_setup(test_tasks_run_in_turn_until_one_ends_the_loop, setup),
        cmocka_unit_test_setup(test_any_error_ends_the_loop, setup),
        cmocka_unit_test_setup(test_watchdog_is_kicked_once_per_round, setup),
        cmocka_unit_test_setup(test_tasks_run_only_when_stopped, setup),
        cmocka_unit_test_setup(test_stopped_after_a_failed_board_setup, setup),
#else
        cmocka_unit_test_setup(test_no_task_resets_at_once, setup),
#endif
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
