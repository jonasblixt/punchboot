#ifndef INCLUDE_LOG_DEF_H
#define INCLUDE_LOG_DEF_H

#include <config.h>
#include <stdio.h>

/* LOGLEVEL is what the console (UART) prints. With CONFIG_RAMLOG, the levels
 * up to CONFIG_RAMLOG_LOGLEVEL are compiled in too, and the ones above
 * LOGLEVEL only go to the RAM log. LOG_MAX_LEVEL is the highest level that is
 * compiled in: use it, not LOGLEVEL, for code that only log messages need. */
#if defined(CONFIG_RAMLOG) && (CONFIG_RAMLOG_LOGLEVEL > LOGLEVEL)
#include <pb/console.h>
#define LOG_MAX_LEVEL CONFIG_RAMLOG_LOGLEVEL
#else
#define LOG_MAX_LEVEL LOGLEVEL
#endif

/*
 * Every line starts with the time since the platform's timer started, in
 * milliseconds with three decimals ("[290.021]"), 0 before. See
 * log_time_start() in pb/console.h.
 */
void log_prefix(const char *level, const char *func);

#define LOG_LINE(prefix, ...)         \
    do {                              \
        log_prefix(prefix, __func__); \
        printf(__VA_ARGS__);          \
        printf("\n\r");               \
    } while (0)

#define LOG_LINE_RAMLOG(prefix, ...)   \
    do {                               \
        console_uart_mute(true);       \
        LOG_LINE(prefix, __VA_ARGS__); \
        console_uart_mute(false);      \
    } while (0)

#if LOGLEVEL >= 2
#define LOG_INFO(...) LOG_LINE("I", __VA_ARGS__)
#elif LOG_MAX_LEVEL >= 2
#define LOG_INFO(...) LOG_LINE_RAMLOG("I", __VA_ARGS__)
#else
#define LOG_INFO(...)
#endif

#if LOGLEVEL >= 3
#define LOG_DBG(...) LOG_LINE("D", __VA_ARGS__)
#elif LOG_MAX_LEVEL >= 3
#define LOG_DBG(...) LOG_LINE_RAMLOG("D", __VA_ARGS__)
#else
#define LOG_DBG(...)
#endif

#if LOGLEVEL >= 1
#define LOG_WARN(...) LOG_LINE("W", __VA_ARGS__)
#define LOG_ERR(...)  LOG_LINE("E", __VA_ARGS__)
#elif LOG_MAX_LEVEL >= 1
#define LOG_WARN(...) LOG_LINE_RAMLOG("W", __VA_ARGS__)
#define LOG_ERR(...)  LOG_LINE_RAMLOG("E", __VA_ARGS__)
#else
#define LOG_WARN(...)
#define LOG_ERR(...)
#endif

#endif
