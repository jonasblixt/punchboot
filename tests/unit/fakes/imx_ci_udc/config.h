/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The host's config.h with a command mode buffer of 64 KiB, for the test of
 * the ChipIdea's driver: the driver has a transfer descriptor per 20 KiB of
 * it, and the host's 4 KiB gives it one.
 */

#ifndef TESTS_UNIT_FAKES_IMX_CI_UDC_CONFIG_H
#define TESTS_UNIT_FAKES_IMX_CI_UDC_CONFIG_H

#include_next <config.h>

#undef CONFIG_CM_BUF_SIZE_KiB
#define CONFIG_CM_BUF_SIZE_KiB 64

#endif // TESTS_UNIT_FAKES_IMX_CI_UDC_CONFIG_H
