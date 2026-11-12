/**
 * Punch BOOT
 *
 * Copyright (C) 2026 ACTIA Nordic AB
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Wrapper header for the Arm Trusted Firmware interface.
 */
#ifndef BOOT_ATF_INTERFACE_H
#define BOOT_ATF_INTERFACE_H

#include <stdint.h>

/* TF-A's utils_def_exp.h defines these as pb/utils_def.h does, in other
 * words: use TF-A's */
#undef U
#undef UL
#undef ULL
#undef L
#undef LL

/*
 * TF-A's headers pick an entry point's layout by __aarch64__. The delivery
 * is AArch64's (boot.md 5.3), so that layout is the one, also where the
 * compiler isn't AArch64's: the host's tests see what BL31 reads.
 */
#ifdef __aarch64__
#include "atf-interface/common/bl_common_exp.h"
#else
#define __aarch64__ 1
#include "atf-interface/common/bl_common_exp.h"
#undef __aarch64__
#endif

#endif /* BOOT_ATF_INTERFACE_H */
