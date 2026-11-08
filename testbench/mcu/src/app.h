/*
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The test application for the MCU test bench (README.md).
 */

#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The registers at the reset handler, saved by start.S */
struct entry_state {
    uint32_t r0; /* pb: the booted system's name, "A" or "B" */
    uint32_t r1; /* pb: the boot partition's UUID, 16 bytes */
    uint32_t r2;
    uint32_t r3;
    uint32_t msp;
    uint32_t psp;
    uint32_t control;
    uint32_t primask;
    uint32_t faultmask;
    uint32_t basepri;
};

extern struct entry_state entry;
extern const uint32_t vectors[];
extern uint32_t _stack_top;

static inline uint32_t mmio_read_32(uintptr_t a)
{
    return *(volatile uint32_t *)a;
}

static inline void mmio_write_32(uintptr_t a, uint32_t v)
{
    *(volatile uint32_t *)a = v;
}

/* Board: board/<board>/board.c */
void board_putc(char c);
/* Core clock, for the delays */
uint32_t board_core_hz(void);
void board_wdog_kick(void);
/* Whether the watchdog runs, and its timeout */
bool board_wdog_running(uint32_t *timeout_ms);
/* The boot state partitions: read, erase and write 'len' bytes at 'offset' */
int board_state_read(unsigned int copy, void *buf, size_t len);
int board_state_write(unsigned int copy, const void *buf, size_t len);
/* Reset; with 'command_mode', pb stays in command mode */
void board_reset(bool command_mode) __attribute__((noreturn));
/* A clock that runs through a warm reset, in us, and words that survive it:
 * for the boot time (APP_MODE_BOOTTIME). 0 without such a clock. */
uint64_t board_reset_clock_us(void);
uint32_t board_persist_read(unsigned int index);
void board_persist_write(unsigned int index, uint32_t value);
#define BOARD_PERSIST_WORDS 3
/* Board specific checks of the state pb left, after the common ones */
void board_checks(void);
extern const uint8_t board_sys_a_uuid[16];
extern const uint8_t board_sys_b_uuid[16];

/* main.c */
void print(const char *s);
void print_hex(uint32_t v);
void print_dec(uint32_t v);
void check(const char *name, bool pass, const char *detail, uint32_t value);
void delay_ms(uint32_t ms);

/* pbstate.c: set the system's verified bit, like pbstate -v */
int pbstate_set_verified(char system);
/* The boot state as pb left it: "MCUTEST STATE ..." */
void pbstate_report(void);

#endif
