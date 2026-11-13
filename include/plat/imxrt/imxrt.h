#ifndef INCLUDE_PLAT_IMXRT_IMXRT_H
#define INCLUDE_PLAT_IMXRT_IMXRT_H

#include <pb/rot.h>
#include <pb/slc.h>
#include <stdint.h>

#include "clock.h"
#include "fusebox.h"
#include "mm.h"
#include "pins.h"

/* SRC (RM 21.6) */
#define IMXRT_SRC_SBMR2                      (IMXRT_SRC_BASE + 0x1C)
#define IMXRT_SRC_SBMR2_BMOD_MASK            (0x3 << 24) /* BMOD[1:0], the boot mode */
#define IMXRT_SRC_GPR5                       (IMXRT_SRC_BASE + 0x30)

/* USB analog (RM 43.5.4) */
#define IMXRT_USB1_CHRG_DETECT_STAT          (IMXRT_ANATOP_BASE + 0x1D0)
#define IMXRT_CHRG_DETECT_STAT_CHRG_DETECTED BIT(1)

/* The boot image's IVT in the FlexSPI NOR, at the ROM's offset (RM Table 9-36) */
#define IMXRT_FLEXSPI_NOR_IVT                (IMXRT_FLEXCIPHER_BASE + 0x1000)

/*
 * The boot reason (plat_get_boot_reason(), platform.md 3.4): raw is WDOG1's
 * WRSR (RM 57.8.1.4; the bits are WDOG_WRSR_* in drivers/wdog/imx_wdog.h) as
 * plat_early_init() read it. The register is read only: nothing clears it.
 * The priority, the highest first, with the kind and the name:
 *
 *   TOUT (bit 1)  WDOG, "WDOG": the watchdog's time-out
 *   SFTW (bit 0)  SW, "SW": WDOG1's software reset, which plat_reset() is
 *   POR (bit 4)   POR, "POR"
 *   none of them  UNKNOWN, "Unknown"
 *
 * WRSR has the watchdog's resets and the power on only. A reset from another
 * source (JTAG, the core's lockup, the temperature sensor: SRC_SRSR has
 * those, RM 21.6.3) leaves it as it was, so the reason is then the one of an
 * earlier reset.
 */
struct imxrt_platform {
    uint8_t si_rev;
    uint8_t speed_grade;
    unsigned int ahb_root_clk_MHz;
    unsigned int ipg_root_clk_MHz;
    unsigned int per_root_clk_MHz;
};

#endif
