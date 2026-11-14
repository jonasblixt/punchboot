/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What pb/assert.h calls, for the unit tests: an assertion of the code under
 * test fails the test that ran it, or is what the test expects
 * (expect_assert_failure()).
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <cmocka.h>

#include <pb/assert.h>

void __assert(const char *file, unsigned int line)
{
    mock_assert(0, "assert() of the code under test", file, (int)line);
}
