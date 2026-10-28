/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Console (src/console.c) with CONFIG_RAMLOG (host config): output from
 * before console_init() only goes to the RAM log, and console_init() writes
 * it to the UART. Log messages above LOGLEVEL only go to the RAM log.
 */

/* The UART prints warnings and errors, the RAM log everything
 * (CONFIG_RAMLOG_LOGLEVEL is 3 in the host config) */
#undef LOGLEVEL
#define LOGLEVEL 1

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cmocka.h>

#include <pb/console.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/ramlog.h>

#define LOG_SIZE  (CONFIG_RAMLOG_SIZE_KiB * 1024)
#define UART_BASE 0x1234

static char uart[LOG_SIZE * 4];
static size_t uart_len;

/* For the boot banner. On the host it goes to the system printf, not the
 * console. */
int plat_boot_reason(void)
{
    return 0;
}

const char *plat_boot_reason_str(void)
{
    return "test";
}

void arch_clean_cache_range(uintptr_t start, size_t len);

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    (void)start;
    (void)len;
}

static void uart_putc(uintptr_t base, char c)
{
    assert_int_equal(base, UART_BASE);
    assert_true(uart_len < sizeof(uart));
    uart[uart_len++] = c;
}

static const struct console_ops uart_ops = {
    .putc = uart_putc,
};

static const struct console_ops no_putc_ops = {
    .putc = NULL,
};

static void puts_console(const char *s)
{
    while (*s)
        console_putc(*s++);
}

static void assert_uart(const char *expected)
{
    assert_int_equal(uart_len, strlen(expected));
    assert_memory_equal(uart, expected, uart_len);
}

static void assert_ramlog(const char *expected)
{
    char buf[256];

    assert_int_equal(ramlog_read(0, buf, sizeof(buf)), strlen(expected));
    assert_memory_equal(buf, expected, strlen(expected));
}

static FILE *real_stdout;

static ssize_t console_stream_write(void *cookie, const char *buf, size_t size)
{
    (void)cookie;

    for (size_t i = 0; i < size; i++)
        console_putc(buf[i]);

    return (ssize_t)size;
}

/* printf() and the LOG macros go through the console, like on target. Only
 * between these two, so that cmocka's output doesn't. */
static void capture_begin(void)
{
    static const cookie_io_functions_t io = { .write = console_stream_write };
    FILE *f = fopencookie(NULL, "w", io);

    assert_non_null(f);
    setvbuf(f, NULL, _IONBF, 0);
    real_stdout = stdout;
    stdout = f;
}

static void capture_end(void)
{
    FILE *f = stdout;

    stdout = real_stdout;
    fclose(f);
}

/* Each test starts without a console, like pb before console_init(). The
 * console with a UART in between clears what the last test left of the muted
 * messages from before console_init(). */
static int setup(void **state)
{
    (void)state;
    console_uart_mute(false);
    console_init(0, NULL);
    console_init(UART_BASE, &uart_ops);
    console_init(0, NULL);
    ramlog_init();
    uart_len = 0;
    return 0;
}

static void test_early_output_is_flushed_on_init(void **state)
{
    (void)state;

    puts_console("early");
    assert_int_equal(uart_len, 0);
    assert_ramlog("early");

    console_init(UART_BASE, &uart_ops);
    assert_uart("early");

    puts_console(" late");
    assert_uart("early late");
    assert_ramlog("early late");
}

static void test_reinit_does_not_flush_again(void **state)
{
    (void)state;

    puts_console("a");
    console_init(UART_BASE, &uart_ops);
    puts_console("b");
    console_init(UART_BASE, &uart_ops);
    assert_uart("ab");
}

/* Boards without a UART: everything stays in the RAM log */
static void test_no_uart_only_ramlog(void **state)
{
    (void)state;

    console_init(0, &no_putc_ops);
    puts_console("x");
    assert_int_equal(uart_len, 0);
    assert_ramlog("x");
}

/* A UART that comes after a console_init() without one gets it all */
static void test_uart_after_null_init(void **state)
{
    (void)state;

    puts_console("1");
    console_init(0, &no_putc_ops);
    puts_console("2");
    console_init(UART_BASE, &uart_ops);
    assert_uart("12");
}

/* More early output than the log holds: the newest LOG_SIZE bytes are written */
static void test_early_output_wrapped(void **state)
{
    const size_t total = LOG_SIZE + 100;
    (void)state;

    for (size_t i = 0; i < total; i++)
        console_putc((char)(i % 251));

    console_init(UART_BASE, &uart_ops);
    assert_int_equal(uart_len, LOG_SIZE);
    for (size_t i = 0; i < LOG_SIZE; i++)
        assert_int_equal((uint8_t)uart[i], (total - LOG_SIZE + i) % 251);
}

static void test_levels(void **state)
{
    (void)state;

    console_init(UART_BASE, &uart_ops);
    capture_begin();
    LOG_DBG("dbg %i", 1);
    LOG_INFO("info");
    LOG_ERR("err");
    printf("plain\n\r");
    capture_end();

    assert_uart("E test_levels: err\n\rplain\n\r");
    assert_ramlog("D test_levels: dbg 1\n\rI test_levels: info\n\r"
                  "E test_levels: err\n\rplain\n\r");
}

/* Muted messages from before console_init() are left out of the replay */
static void test_early_muted(void **state)
{
    (void)state;

    capture_begin();
    LOG_DBG("a");
    LOG_ERR("b");
    LOG_INFO("c");
    printf("d\n\r");
    LOG_DBG("e");
    capture_end();

    console_init(UART_BASE, &uart_ops);
    assert_uart("E test_early_muted: b\n\rd\n\r");
}

/* A muted message that starts before the oldest byte still in the log */
static void test_early_muted_wrapped(void **state)
{
    (void)state;

    console_uart_mute(true);
    for (size_t i = 0; i < LOG_SIZE + 10; i++)
        console_putc('m');
    console_uart_mute(false);
    puts_console("ok");

    console_init(UART_BASE, &uart_ops);
    assert_uart("ok");
}

/* The muted messages past the last slot are replayed. Unmuting when not muted
 * (exception handlers) changes nothing. */
static void test_early_muted_overflow(void **state)
{
    char expected[64] = { 0 };
    (void)state;

    console_uart_mute(false);
    for (int i = 0; i < 33; i++) {
        console_uart_mute(true);
        console_putc('m');
        console_uart_mute(false);
        console_uart_mute(false);
        console_putc('u');
    }

    console_init(UART_BASE, &uart_ops);
    memset(expected, 'u', 32);
    strcat(expected, "mu");
    assert_uart(expected);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup(test_early_output_is_flushed_on_init, setup),
        cmocka_unit_test_setup(test_reinit_does_not_flush_again, setup),
        cmocka_unit_test_setup(test_no_uart_only_ramlog, setup),
        cmocka_unit_test_setup(test_uart_after_null_init, setup),
        cmocka_unit_test_setup(test_early_output_wrapped, setup),
        cmocka_unit_test_setup(test_levels, setup),
        cmocka_unit_test_setup(test_early_muted, setup),
        cmocka_unit_test_setup(test_early_muted_wrapped, setup),
        cmocka_unit_test_setup(test_early_muted_overflow, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
