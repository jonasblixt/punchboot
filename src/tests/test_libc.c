#include <stdio.h>
#include <plat/test/semihosting.h>
#include <usb.h>
#include <assert.h>
#include <string.h>
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


uint32_t  plat_usb_transfer (struct usb_device *dev, uint8_t ep, 
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

uint32_t plat_prepare_recovery(void)
{
    return PB_OK;
};

void test_main(void) 

{
    char s[256];

    LOG_INFO("Boot libc");

    snprintf(s,256,"%i", -1234567890);
    assert (strcmp(s,"-1234567890") == 0);

    snprintf(s,256,"%08x", 0x0000CAFE);
    LOG_INFO("%s",s);
    assert (strcmp(s,"0000CAFE") == 0);


    snprintf(s,256,"%x", 0xCAFEBABE);
    assert (strcmp(s,"CAFEBABE") == 0);
    LOG_INFO("Boot libc end");
}
