/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The comparison a board's password_auth() makes (board.md 4.4). A board
 * that uses it adds src/board/password.c to its src-y.
 */

#ifndef SRC_BOARD_PASSWORD_H
#define SRC_BOARD_PASSWORD_H

#include <stddef.h>

/**
 * Compare the password a host sent with the expected one: the whole
 * password, and the lengths match too. The time it takes depends on the
 * length the host sent only, not on what the bytes are or on how long the
 * expected password is. Nothing is logged.
 *
 * @param expected        The secret, expected_length bytes
 * @param expected_length Its length, at least 1
 * @param password        The host's password, length bytes, not NUL terminated
 * @param length          Its length
 *
 * @return PB_OK when they are the same, or -PB_ERR_AUTHENTICATION_FAILED
 */
int board_password_check(const char *expected,
                         size_t expected_length,
                         const char *password,
                         size_t length);

#endif // SRC_BOARD_PASSWORD_H
