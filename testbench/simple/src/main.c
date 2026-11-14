/*
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Checks what pb handed over (the registers and the transfer list of
 * Firmware Handoff, the CPU's state) and reports it on the console, one line
 * per check, then does what the image was built for (APP_MODE_*):
 *
 *   good      mark the system verified, then reset into pb's command mode
 *   noverify  don't mark it, reset (pb boots it again, a boot attempt)
 *   hang      stop petting the watchdog
 *   boottime  time the warm reset to its own entry APP_BOOT_RUNS times, with
 *             a clock that runs through the reset (APP_BOOT_RUNS runs)
 *
 * Every line starts with "MCUTEST ", tests/ parses them.
 */

#include <boot/pb_fh.h>

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

static bool is_name(const char *s, char *system)
{
    if ((s[0] == 'A' || s[0] == 'B') && s[1] == 0) {
        *system = s[0];
        return true;
    }
    return false;
}

static bool uuid_equal(const uint8_t *a, const uint8_t *uu)
{
    for (int i = 0; i < 16; i++) {
        if (a[i] != uu[i])
            return false;
    }
    return true;
}

/* The list pb writes for this board: the list's header, the entry's, pb's
 * structure and the padding to the next multiple of 8 */
#define LIST_USED_SIZE                                                \
    (sizeof(struct pb_fh_tl_header) + sizeof(struct pb_fh_tl_entry) + \
     ((sizeof(struct pb_fh_handover) + 7) & ~(size_t)7))

/* The entry after 'last' in the list's used part, the first one with NULL,
 * or NULL at its end: an entry starts at the next multiple of 8 after the
 * one before it */
static const struct pb_fh_tl_entry *next_entry(const struct pb_fh_tl_header *tl,
                                               const struct pb_fh_tl_entry *last)
{
    uint32_t end = (uint32_t)tl + tl->used_size;
    uint32_t at;

    if (last)
        at = ((uint32_t)last + PB_FH_TL_ENTRY_HDR_SIZE(last) + last->data_size + 7) & ~7u;
    else
        at = (uint32_t)tl + tl->hdr_size;

    const struct pb_fh_tl_entry *te = (const struct pb_fh_tl_entry *)at;

    if (at + sizeof(*te) > end || PB_FH_TL_ENTRY_HDR_SIZE(te) < sizeof(*te) ||
        PB_FH_TL_ENTRY_HDR_SIZE(te) > end - at ||
        te->data_size > end - at - PB_FH_TL_ENTRY_HDR_SIZE(te))
        return NULL;
    return te;
}

/*
 * The transfer list pb handed over (Firmware Handoff, include/boot/pb_fh.h):
 * its header and its checksum as a receiver checks them, and its one entry,
 * pb's structure. NULL when there is none to read. The list is only read
 * where the registers say it is the board's region, which is RAM: an image
 * that pb started with other registers reports that and doesn't fault.
 */
static const struct pb_fh_handover *list_checks(void)
{
    const struct pb_fh_tl_header *tl = (const struct pb_fh_tl_header *)entry.r3;
    const struct pb_fh_tl_entry *found = NULL;
    uint32_t entries = 0;
    uint32_t fdts = 0;
    uint8_t sum = 0;
    bool sizes;

    /* AArch32's registers, which pb also uses on ARMv7-M */
    check("fh-r0", entry.r0 == 0, "r0", entry.r0);
    check("fh-r1", entry.r1 == PB_FH_REG_R1, "r1", entry.r1);
    /* The board's pb names no device tree part */
    check("fh-r2", entry.r2 == 0, "r2", entry.r2);
    /* The region this image's memory map keeps free for it (board.mk) */
    check("fh-r3", entry.r3 == APP_FH_REGION_BASE, "r3", entry.r3);

    if (entry.r1 != PB_FH_REG_R1 || entry.r3 != APP_FH_REGION_BASE) {
        check("fh-signature", false, "not read", 0);
        return NULL;
    }

    check("fh-signature", tl->signature == PB_FH_TL_SIGNATURE, "signature", tl->signature);
    check("fh-version", tl->version == PB_FH_TL_VERSION, "version", tl->version);
    check("fh-hdr-size", tl->hdr_size == sizeof(*tl), "hdr_size", tl->hdr_size);
    check("fh-alignment", tl->alignment == PB_FH_TL_ALIGNMENT, "alignment", tl->alignment);
    check("fh-total-size", tl->total_size == APP_FH_REGION_SIZE, "total_size", tl->total_size);
    sizes = tl->used_size >= sizeof(*tl) && tl->used_size <= tl->total_size &&
            (tl->used_size % 8) == 0 && tl->total_size <= APP_FH_REGION_SIZE;
    /* The image is built from the tree whose pb it tests, so it knows the
     * list to the byte: the two headers, pb's structure and its padding to
     * the next multiple of 8. An image in the field reads what the sizes say */
    check("fh-used-size", sizes && tl->used_size == LIST_USED_SIZE, "used_size", tl->used_size);
    check("fh-flags", tl->flags == PB_FH_TL_FLAG_HAS_CHECKSUM, "flags", tl->flags);

    if (tl->signature != PB_FH_TL_SIGNATURE || tl->hdr_size != sizeof(*tl) || !sizes) {
        check("fh-checksum", false, "not read", 0);
        return NULL;
    }

    /* The used bytes sum to 0 modulo 256 */
    for (uint32_t i = 0; i < tl->used_size; i++)
        sum += ((const uint8_t *)tl)[i];
    check("fh-checksum", sum == 0, "sum", sum);

    for (const struct pb_fh_tl_entry *te = next_entry(tl, NULL); te; te = next_entry(tl, te)) {
        entries++;
        if (PB_FH_TL_ENTRY_TAG(te) == PB_FH_TAG_FDT)
            fdts++;
        if (PB_FH_TL_ENTRY_TAG(te) == PB_FH_TAG_HANDOVER && !found)
            found = te;
    }

    /* pb's structure or a device tree, never both: here pb's structure */
    check("fh-entries", entries == 1, "entries", entries);
    check("fh-no-fdt", fdts == 0, "FDT entries", fdts);
    check("fh-entry", found != NULL, "tag", PB_FH_TAG_HANDOVER);

    if (!found)
        return NULL;

    /* The data follows the entry's header, whose size the entry says: 8 in
     * this version of the list. With another size the structure isn't where
     * this image would read it */
    check("fh-entry-hdr-size",
          PB_FH_TL_ENTRY_HDR_SIZE(found) == sizeof(*found),
          "hdr_size",
          PB_FH_TL_ENTRY_HDR_SIZE(found));

    /* pb's structure of this tree, no smaller and no larger */
    check("fh-data-size",
          found->data_size == sizeof(struct pb_fh_handover),
          "data_size",
          found->data_size);

    if (PB_FH_TL_ENTRY_HDR_SIZE(found) != sizeof(*found) ||
        found->data_size < sizeof(struct pb_fh_handover))
        return NULL;

    return (const struct pb_fh_handover *)((const uint8_t *)found + PB_FH_TL_ENTRY_HDR_SIZE(found));
}

/* pb's information in pb's structure (boot.md 5.6) */
static void handover_checks(const struct pb_fh_handover *h, char *system)
{
    uint32_t length = 0;

    if (h == NULL) {
        check("system-name", false, "no structure", 0);
        check("system-uuid", false, "no structure", 0);
        return;
    }

    check("system-name", is_name(h->system_name, system), "first byte", (uint8_t)h->system_name[0]);
    if (*system != '?') {
        const uint8_t *uu = (*system == 'A') ? board_sys_a_uuid : board_sys_b_uuid;
        check("system-uuid",
              uuid_equal(h->boot_partition_uuid, uu),
              "first bytes",
              (uint32_t)h->boot_partition_uuid[0] << 24 | h->boot_partition_uuid[1] << 16 |
                  h->boot_partition_uuid[2] << 8 | h->boot_partition_uuid[3]);
    } else {
        check("system-uuid", false, "no system", 0);
    }

    check("boot-mode", h->boot_mode <= PB_FH_BOOT_MODE_SPECULATIVE, "boot_mode", h->boot_mode);
    check("keys", h->no_of_available_keys <= PB_FH_MAX_KEYS, "keys", h->no_of_available_keys);

    while (length < sizeof(h->version) && h->version[length])
        length++;
    check("version", length > 0 && length < sizeof(h->version), "length", length);

    info("slc", "slc", (uint32_t)h->slc);
    info("active-key", "key id", h->active_key);
    info("status", "status", h->status);
    if (length < sizeof(h->version)) {
        print("MCUTEST VERSION ");
        print(h->version);
        print("\n");
    }
}

static void entry_checks(char *system)
{
    uint32_t iser = 0;
    uint32_t wdog_ms = 0;

    *system = '?';
    handover_checks(list_checks(), system);
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

    /* include/pb/arch.h: interrupts masked, the D-cache and the MPU off */
    check("primask", entry.primask == 1, "PRIMASK", entry.primask);
    check("dcache", (mmio_read_32(SCB_CCR) & CCR_DC) == 0, "CCR", mmio_read_32(SCB_CCR));
    check("mpu", mmio_read_32(MPU_CTRL) == 0, "MPU_CTRL", mmio_read_32(MPU_CTRL));
    info("basepri", "BASEPRI", entry.basepri);
    info("icache", "CCR IC (bit 17)", mmio_read_32(SCB_CCR) & CCR_IC);

    board_checks();
}

#if APP_MODE_BOOTTIME
/* The persistent words: a magic, the clock at the reset request (us, 32 bit
 * is enough for a boot) and the runs so far */
#define BOOTTIME_MAGIC    0x50424254 /* "PBBT" */
/* The longest a boot can take without a watchdog to bound it */
#define BOOTTIME_LIMIT_US (60u * 1000000u)

/*
 * The words describe the reset this image asked for, and only until its next
 * entry: they are read and cleared first. Words that a series left when it
 * didn't end (a reset that didn't come back here, the board reset or powered
 * in between) gave the next series' first boot the old reset's clock: one
 * boot of 3262 s (F-171). A time longer than a boot can take, the watchdog's
 * timeout, is such words: not a measurement, and the series starts over.
 */
static void boottime(uint64_t entry_us)
{
    uint32_t runs = 0;

    if (board_persist_read(0) == BOOTTIME_MAGIC) {
        uint32_t us = (uint32_t)entry_us - board_persist_read(1);
        uint32_t wdog_ms = 0;
        uint32_t limit_us = board_wdog_running(&wdog_ms) ? wdog_ms * 1000u : BOOTTIME_LIMIT_US;

        board_persist_write(0, 0);

        if (us <= limit_us) {
            runs = board_persist_read(2);
            print("MCUTEST BOOTTIME ");
            print_dec(us);
            print(" us\n");
        } else {
            info("boottime-stale", "us", us);
        }
    }

    if (runs < APP_BOOT_RUNS) {
        board_persist_write(0, BOOTTIME_MAGIC);
        board_persist_write(2, runs + 1);
        /* Without words that keep their values it would reset forever */
        if (board_persist_read(0) != BOOTTIME_MAGIC || board_persist_read(2) != runs + 1) {
            check("boottime-persist", false, "read back", board_persist_read(0));
            board_persist_write(0, 0);
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
