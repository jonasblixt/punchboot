/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <plat/test/semihosting.h>
#include <usb.h>
#include "test.h"

uint32_t  plat_usb_init(struct usb_device *dev)
{
    UNUSED(dev);
    return PB_OK;
}

void      plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
    UNUSED(addr);
    UNUSED(dev);
}

void plat_usb_set_configuration(struct usb_device *dev)
{
    UNUSED(dev);
}

void plat_usb_wait_for_ep_completion(uint32_t ep)
{
    UNUSED(ep);
}


uint32_t plat_prepare_recovery(struct pb_platform_setup *plat)
{
    UNUSED(plat);
    return PB_OK;
}

uint32_t  plat_usb_transfer(struct usb_device *dev, uint8_t ep,
                            uint8_t *bfr, uint32_t sz)
{
    UNUSED(dev);
    UNUSED(ep);
    UNUSED(bfr);
    UNUSED(sz);
    return PB_OK;
}

void plat_usb_task(struct usb_device *dev)
{
    UNUSED(dev);
}
void test_main(void)

{
    LOG_INFO("Boot test");
    LOG_INFO("Boot test end");
}
