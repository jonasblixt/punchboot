/*
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Marks the booted system verified in pb's boot state, what pbstate's
 * pbstate_set_system_verified() does on Linux: the primary copy, or the
 * backup when the primary is bad, gets the verified bit and no remaining
 * boot attempts, then both copies are written.
 */

#include <boot/pb_state_blob.h>
#include <pb/crc.h>

#include "app.h"

static struct pb_boot_state state __attribute__((aligned(4)));

static bool state_valid(void)
{
    uint32_t crc = state.crc;

    state.crc = 0;
    bool ok = state.magic == PB_STATE_MAGIC &&
              crc == crc32(0, (const uint8_t *)&state, sizeof(state));
    state.crc = crc;
    return ok;
}

static int read_copy(unsigned int copy)
{
    int rc = board_state_read(copy, &state, sizeof(state));

    if (rc != 0)
        return rc;
    return state_valid() ? 0 : -1;
}

void pbstate_report(void)
{
    print("MCUTEST STATE ");
    if (read_copy(0) != 0 && read_copy(1) != 0) {
        print("invalid\n");
        return;
    }
    print("enable ");
    print_hex(state.enable);
    print(" verified ");
    print_hex(state.verified);
    print(" attempts ");
    print_dec(state.remaining_boot_attempts);
    print(" error ");
    print_hex(state.error);
    print("\n");
}

int pbstate_set_verified(char system)
{
    int rc;

    if (read_copy(0) != 0 && read_copy(1) != 0)
        return -2;

    state.verified |= (system == 'A') ? PB_STATE_A_VERIFIED : PB_STATE_B_VERIFIED;
    state.remaining_boot_attempts = 0;
    state.crc = 0;
    state.crc = crc32(0, (const uint8_t *)&state, sizeof(state));

    for (unsigned int copy = 0; copy < 2; copy++) {
        rc = board_state_write(copy, &state, sizeof(state));
        if (rc != 0)
            return -10 - (int)copy;
    }

    /* Read back what pb will read */
    return read_copy(0) == 0 ? 0 : -3;
}
