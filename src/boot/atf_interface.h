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

#include "atf-interface/common/bl_common_exp.h"

#endif /* BOOT_ATF_INTERFACE_H */
