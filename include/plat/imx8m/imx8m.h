#ifndef INCLUDE_PLAT_IMX8M_H
#define INCLUDE_PLAT_IMX8M_H

#include "clock.h"
#include "hab.h"
#include "mm.h"
#include "pins.h"

struct imx8m_platform {};

int board_init(struct imx8m_platform *plat);
void board_console_init(struct imx8m_platform *plat);

#endif
