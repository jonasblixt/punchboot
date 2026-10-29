#ifndef INCLUDE_CONSOLE_H
#define INCLUDE_CONSOLE_H

#include <inttypes.h>
#include <stdbool.h>

struct console_ops {
    void (*putc)(uintptr_t base, char c);
};

void console_putc(char c);
void console_init(uintptr_t base, const struct console_ops *ops);

/**
 * Mute the console's UART output, for messages that only go to the RAM log.
 * The LOG_* macros use it for levels above LOGLEVEL, up to
 * CONFIG_RAMLOG_LOGLEVEL. Unmuting when not muted does nothing.
 *
 * @param[in] mute True to mute, false to unmute
 */
void console_uart_mute(bool mute);

/**
 * The log's clock starts: from here on log lines carry the time since this
 * call, before it they show 0. The platform calls it once plat_get_us_tick()
 * works.
 */
void log_time_start(void);

#endif
