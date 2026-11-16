/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A wait on a register without the timer (drivers.md 3, F-462). For the waits
 * where pb_timeout_*() can't be used: a console that prints before the timer
 * runs (the 8M SPL's), and the RT's clock switches, where a call out of the
 * loop faulted while the AHB root switched (code fetched during the switch
 * read wrong). The bound is a count of reads, sized at 100 times the measured
 * wait at 10 ns a read: a register read takes longer on every port, so the
 * bound in time is longer too, and still far below the watchdog.
 */

#ifndef INCLUDE_PB_MMIO_POLL_H
#define INCLUDE_PB_MMIO_POLL_H

#include <pb/errors.h>
#include <pb/mmio.h>
#include <stdint.h>

/* A PLL's lock: the RT's ARM PLL took 54 us (F-462) */
#define MMIO_POLL_PLL_LOCK  1000000u
/* A CCM divider's or mux's handshake (CDHIPR): 4 to 5 us on the RT; a
 * controller's own reset */
#define MMIO_POLL_HANDSHAKE 100000u
/* A UART's character: 87 us at 115200 baud, 1 ms at 9600 */
#define MMIO_POLL_UART_CHAR 1000000u

/* Wait until (register & mask) == value: PB_OK, or -PB_ERR_TIMEOUT after
 * 'reads' reads. Always inline: the loop calls nothing, also at -Os */
static inline __attribute__((always_inline)) int
mmio_poll_32(uintptr_t addr, uint32_t mask, uint32_t value, uint32_t reads)
{
    while (reads-- > 0) {
        if ((mmio_read_32(addr) & mask) == value)
            return PB_OK;
    }

    return -PB_ERR_TIMEOUT;
}

#endif /* INCLUDE_PB_MMIO_POLL_H */
