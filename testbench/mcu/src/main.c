/*
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Checks what pb handed over and reports it on the console, one line per
 * check, then does what the image was built for (APP_MODE_*):
 *
 *   good      mark the system verified, then reset into pb's command mode
 *   noverify  don't mark it, reset (pb boots it again, a boot attempt)
 *   hang      stop petting the watchdog
 *   boottime  time the warm reset to its own entry APP_BOOT_RUNS times, with
 *             a clock that runs through the reset (APP_BOOT_RUNS runs)
 *
 * Every line starts with "MCUTEST ", tests/ parses them.
 */

#include "app.h"

/* .data, start.S writes it before .bss is cleared */
struct entry_state entry __attribute__((section(".data"))) = { 0 };

static unsigned int checks_run;
static unsigned int checks_failed;

/* Cortex-M registers */
#define SCB_ICSR    0xe000ed04
#define SCB_VTOR    0xe000ed08
#define SCB_CCR     0xe000ed14
#define SYST_CSR    0xe000e010
#define NVIC_ISER0  0xe000e100
#define MPU_CTRL    0xe000ed94
#define DEMCR       0xe000edfc
#define DWT_CTRL    0xe0001000
#define DWT_CYCCNT  0xe0001004

#define CCR_DC      (1u << 16)
#define CCR_IC      (1u << 17)
#define NVIC_ISER_N 8

void print(const char *s)
{
    while (*s) {
        if (*s == '\n')
            board_putc('\r');
        board_putc(*s++);
    }
}

void print_hex(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";

    print("0x");
    for (int i = 28; i >= 0; i -= 4)
        board_putc(digits[(v >> i) & 0xf]);
}

void print_dec(uint32_t v)
{
    char buf[11];
    int i = sizeof(buf) - 1;

    buf[i] = 0;
    do {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    print(&buf[i]);
}

void check(const char *name, bool pass, const char *detail, uint32_t value)
{
    checks_run++;
    if (!pass)
        checks_failed++;

    print("MCUTEST CHECK ");
    print(name);
    print(pass ? " PASS " : " FAIL ");
    print(detail);
    print(" ");
    print_hex(value);
    print("\n");
}

static void info(const char *name, const char *detail, uint32_t value)
{
    print("MCUTEST INFO ");
    print(name);
    print(" ");
    print(detail);
    print(" ");
    print_hex(value);
    print("\n");
}

void delay_ms(uint32_t ms)
{
    uint32_t per_ms = board_core_hz() / 1000;

    for (uint32_t i = 0; i < ms; i++) {
        uint32_t start = mmio_read_32(DWT_CYCCNT);

        while (mmio_read_32(DWT_CYCCNT) - start < per_ms)
            ;
        if ((i % 100) == 0 && APP_KICK_WDOG)
            board_wdog_kick();
    }
}

/* An exception or interrupt: the image enables none */
void unexpected_exception(uint32_t ipsr)
{
    print("MCUTEST FAIL unexpected exception ");
    print_dec(ipsr & 0x1ff);
    print("\n");
    for (;;)
        ;
}

static bool is_name(uint32_t p, char *system)
{
    const char *s = (const char *)p;

    /* The name is in pb's RAM, which this image doesn't overwrite. A bad
     * pointer ends up in unexpected_exception(), with the fault. */
    if (p == 0)
        return false;
    if ((s[0] == 'A' || s[0] == 'B') && s[1] == 0) {
        *system = s[0];
        return true;
    }
    return false;
}

static bool uuid_equal(uint32_t p, const uint8_t *uu)
{
    const uint8_t *a = (const uint8_t *)p;

    if (p == 0)
        return false;
    for (int i = 0; i < 16; i++) {
        if (a[i] != uu[i])
            return false;
    }
    return true;
}

static void entry_checks(char *system)
{
    uint32_t iser = 0;
    uint32_t wdog_ms = 0;

    *system = '?';
    check("system-name", is_name(entry.r0, system), "r0", entry.r0);
    if (*system != '?') {
        const uint8_t *uu = (*system == 'A') ? board_sys_a_uuid : board_sys_b_uuid;
        check("system-uuid", uuid_equal(entry.r1, uu), "r1", entry.r1);
    } else {
        check("system-uuid", false, "r1", entry.r1);
    }
    check("vtor", mmio_read_32(SCB_VTOR) == (uint32_t)vectors, "VTOR", mmio_read_32(SCB_VTOR));
    check("msp", entry.msp == (uint32_t)&_stack_top, "MSP", entry.msp);
    check("control", entry.control == 0, "CONTROL", entry.control);
    check("faultmask", entry.faultmask == 0, "FAULTMASK", entry.faultmask);

    for (unsigned int i = 0; i < NVIC_ISER_N; i++)
        iser |= mmio_read_32(NVIC_ISER0 + 4 * i);
    check("nvic", iser == 0, "ISER (or of all)", iser);
    check("systick", (mmio_read_32(SYST_CSR) & 1) == 0, "SYST_CSR", mmio_read_32(SYST_CSR));
    check("pending", (mmio_read_32(SCB_ICSR) & 0x1ff000) == 0, "ICSR", mmio_read_32(SCB_ICSR));
    check("watchdog", board_wdog_running(&wdog_ms), "timeout ms", wdog_ms);

    info("primask", "PRIMASK", entry.primask);
    info("basepri", "BASEPRI", entry.basepri);
    info("caches", "CCR (IC bit 17, DC bit 16)", mmio_read_32(SCB_CCR) & (CCR_IC | CCR_DC));
    info("mpu", "MPU_CTRL", mmio_read_32(MPU_CTRL));

    board_checks();
}

#if APP_MODE_BOOTTIME
/* The persistent words: a magic, the clock at the reset request (us, 32 bit
 * is enough for a boot) and the runs so far */
#define BOOTTIME_MAGIC 0x50424254 /* "PBBT" */

static void boottime(uint64_t entry_us)
{
    uint32_t runs = 0;

    if (board_persist_read(0) == BOOTTIME_MAGIC) {
        uint32_t reset_us = board_persist_read(1);

        runs = board_persist_read(2);
        print("MCUTEST BOOTTIME ");
        print_dec((uint32_t)entry_us - reset_us);
        print(" us\n");
    }

    if (runs < APP_BOOT_RUNS) {
        board_persist_write(0, BOOTTIME_MAGIC);
        board_persist_write(2, runs + 1);
        /* Without words that keep their values it would reset forever */
        if (board_persist_read(0) != BOOTTIME_MAGIC || board_persist_read(2) != runs + 1) {
            check("boottime-persist", false, "read back", board_persist_read(0));
            print("MCUTEST RESET command-mode\n");
            delay_ms(10);
            board_reset(true);
        }
        print("MCUTEST RESET boot\n");
        delay_ms(10);
        /* The clock as late as possible before the reset */
        board_persist_write(1, (uint32_t)board_reset_clock_us());
        board_reset(false);
    }

    board_persist_write(0, 0);
    print("MCUTEST RESET command-mode\n");
    delay_ms(10);
    board_reset(true);
}
#endif

int main(void)
{
    char system;
#if APP_MODE_BOOTTIME
    /* Before anything takes time */
    uint64_t entry_us = board_reset_clock_us();
#endif

    /* The cycle counter, for delay_ms() */
    mmio_write_32(DEMCR, mmio_read_32(DEMCR) | (1u << 24));
    mmio_write_32(DWT_CYCCNT, 0);
    mmio_write_32(DWT_CTRL, mmio_read_32(DWT_CTRL) | 1);

    print("\nMCUTEST START " APP_MODE_NAME " " APP_BOARD "\n");
    entry_checks(&system);
    print("MCUTEST SYSTEM ");
    board_putc(system);
    print("\n");
    pbstate_report();

#if APP_MODE_GOOD || APP_MODE_BOOTTIME
    int rc = (system == '?') ? -1 : pbstate_set_verified(system);
    check("verified", rc == 0, "pbstate", (uint32_t)rc);
#endif

    print("MCUTEST RESULT ");
    print(checks_failed ? "FAIL " : "PASS ");
    print_dec(checks_run - checks_failed);
    print("/");
    print_dec(checks_run);
    print("\n");

#if APP_MODE_GOOD
    delay_ms(APP_HOLD_MS);
    print("MCUTEST RESET command-mode\n");
    delay_ms(10);
    board_reset(true);
#elif APP_MODE_NOVERIFY
    delay_ms(APP_HOLD_MS);
    print("MCUTEST RESET boot\n");
    delay_ms(10);
    board_reset(false);
#elif APP_MODE_BOOTTIME
    boottime(entry_us);
#elif APP_MODE_HANG
    print("MCUTEST HANG\n");
    for (;;)
        ;
#else
#error "APP_MODE_GOOD, _NOVERIFY, _HANG or _BOOTTIME"
#endif
}
