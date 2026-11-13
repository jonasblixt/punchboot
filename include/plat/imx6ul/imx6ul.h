#ifndef PLATFORM_IMX6UL_H
#define PLATFORM_IMX6UL_H

#include <pb/rot.h>
#include <pb/slc.h>
#include <stdint.h>

#include "clock.h"
#include "fusebox.h"
#include "hab.h"
#include "mm.h"
#include "pins.h"

/*
 * The boot reason (plat_get_boot_reason(), platform.md 3.4): raw is SRC_SRSR
 * (RM 49.7.3) as plat_early_init() read it. A reset sets its cause's bit,
 * and only a power on reset or a 1 written to a bit clears one. pb writes
 * none, so every cause survives the resets that follow it: after a power on
 * and a reboot both bits are set, and the kind is the highest priority cause
 * since the OS last cleared the register, not the cause of this reset.
 *
 * The priority is the bit's number, the highest first; a power on is last.
 * The kind and the name of each:
 */
#define IMX6UL_SRSR_TEMPSENSE BIT(8) /* FAULT, "temp": the temperature sensor */
#define IMX6UL_SRSR_WDOG3     BIT(7) /* WDOG, "wdog3" */
#define IMX6UL_SRSR_JTAG_SW   BIT(6) /* DEBUG, "jtag_sw": a software reset over JTAG */
#define IMX6UL_SRSR_JTAG      BIT(5) /* DEBUG, "jtag_rst": the JTAG reset */
/* WDOG, "wdog": a WDOG's time-out, and its software reset too, which is
 * plat_reset() and the OS's reboot: SRSR can't tell them apart */
#define IMX6UL_SRSR_WDOG      BIT(4)
#define IMX6UL_SRSR_USER      BIT(3) /* UNKNOWN, "User": ipp_user_reset_b, no kind fits */
#define IMX6UL_SRSR_CSU       BIT(2) /* FAULT, "CSU": the CSU's alarm */
#define IMX6UL_SRSR_POR       BIT(0) /* POR, "POR": ipp_reset_b */
/* Not a cause: software's flag for a warm reset. It is in raw, and neither
 * the kind nor the name comes from it. No cause set: UNKNOWN, "Unknown" */
#define IMX6UL_SRSR_WARM_BOOT BIT(16)

struct imx6ul_platform {
    uint8_t si_rev;
    uint8_t speed_grade;
    unsigned int core_clk_MHz;
    unsigned int usdhc1_clk_MHz;
    unsigned int usdhc2_clk_MHz;
};

/** The platform's clocks and fuses, for the board */
const struct imx6ul_platform *imx6ul_plat(void);

#endif // PLATFORM_IMX6UL_H
