#include <plat.h>
#include <tinyprintf.h>
#include "ocotp.h"

#define OCOTP_DEBUG

void ocotp_init(struct ocotp_dev *dev) {
#ifdef OCOTP_DEBUG
    tfp_printf("Initializing ocotp at 0x%8.8X\n\r", dev->base);
#endif
}
