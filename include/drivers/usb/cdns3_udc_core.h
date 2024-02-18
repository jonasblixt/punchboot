#ifndef INCLUDE_DRIVERS_USB_CDNS3_UDC_CORE_H
#define INCLUDE_DRIVERS_USB_CDNS3_UDC_CORE_H

#include <drivers/usb/usbd.h>
#include <stdint.h>

int cdns3_udc_core_init(void);
int cdns3_udc_core_stop(void);
int cdns3_udc_core_xfer_zlp(usb_ep_t ep);
int cdns3_udc_core_set_address(uint16_t addr);
int cdns3_udc_set_configuration(const struct usb_endpoint_descriptor *eps, size_t no_of_eps);
int cdns3_udc_core_poll_setup_pkt(struct usb_setup_packet *pkt);
void cdns3_udc_core_xfer_cancel(usb_ep_t ep);
int cdns3_udc_core_xfer_complete(usb_ep_t ep);
int cdns3_udc_core_xfer_start(usb_ep_t ep, void *buf, size_t length);
void cdns3_udc_core_set_base(uintptr_t base_);

#endif
