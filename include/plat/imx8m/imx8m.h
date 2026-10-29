#ifndef INCLUDE_PLAT_IMX8M_H
#define INCLUDE_PLAT_IMX8M_H

#include "clock.h"
#include "mm.h"
#if defined(CONFIG_PLAT_IMX8MQ)
#include "pins_imx8mq.h"
#elif defined(CONFIG_PLAT_IMX8MP)
#include "pins_imx8mp.h"
#endif

struct imx8m_platform {};

int board_init(struct imx8m_platform *plat);
void board_console_init(struct imx8m_platform *plat);

#endif
