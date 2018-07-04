#include <pb.h>
#include <io.h>


uint32_t  plat_usb_init(struct usb_device *dev)
{
}

void      plat_usb_task(struct usb_device *dev)
{
}

uint32_t  plat_usb_transfer (struct usb_device *dev, uint8_t ep, 
                            uint8_t *bfr, uint32_t sz)
{
}

void      plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
}

void      plat_usb_set_configuration(struct usb_device *dev)
{
}

void      plat_usb_wait_for_ep_completion(uint32_t ep)
{
}

