/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX8M SPL's fastboot (D-089): the commands it takes, without the USB.
 * A command is at most FB_CMD_MAX bytes of text, every answer at most
 * FB_REPLY_MAX: "OKAY", "FAIL", "DATA" and their text (Android's fastboot
 * protocol, as uuu's FB: commands send it).
 */

#ifndef PLAT_IMX8M_SPL_FB_PROTO_H
#define PLAT_IMX8M_SPL_FB_PROTO_H

#include <stddef.h>
#include <stdint.h>

#define FB_CMD_MAX   64
#define FB_REPLY_MAX 64

struct fb_state {
    size_t room; /* The most a download may be */
    size_t expect; /* The download announced, its bytes due */
    size_t downloaded; /* The last download's size, 0 before one */
};

enum fb_next {
    FB_NEXT_REPLY, /* Send the reply, read the next command */
    FB_NEXT_DATA, /* Send the reply ("DATA..."), then receive 'expect' bytes */
    FB_NEXT_BOOT, /* Check and start what was downloaded; the caller replies */
};

/**
 * One command
 *
 * @param[in,out] s The session's state
 * @param[in] cmd The command, not terminated
 * @param[in] len Its length
 * @param[out] reply The answer, NUL terminated, at most FB_REPLY_MAX bytes and the NUL
 *
 * @return What the caller does next
 */
enum fb_next fb_command(struct fb_state *s, const char *cmd, size_t len, char *reply);

/**
 * The download's bytes arrived
 *
 * @param[in,out] s The session's state
 * @param[in] ok The transfer was done, all of 'expect'
 * @param[out] reply "OKAY" or "FAIL..."
 */
void fb_data_done(struct fb_state *s, int ok, char *reply);

#endif // PLAT_IMX8M_SPL_FB_PROTO_H
