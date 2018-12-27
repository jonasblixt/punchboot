#include <pb.h>
#include <io.h>
#include <usb.h>
#include <tinyprintf.h>
#include <plat/test/virtio_serial.h>
#include <plat/test/socket_proto.h>

struct virtio_serial_device d;

uint32_t  plat_usb_init(struct usb_device *dev)
{
    UNUSED(dev);

    d.dev.device_id = 3;
    d.dev.vendor_id = 0x554D4551;
    d.dev.base = 0x0A003E00;

    if (virtio_serial_init(&d) != PB_OK)
    {
        LOG_ERR("Could not initialize virtio serial port");
        while(1);
    }
    return PB_OK;
}

void      plat_usb_task(struct usb_device *dev)
{
    struct pb_socket_header hdr;
    struct usb_pb_command cmd;
    uint8_t status = 0;

    UNUSED(dev);
    virtio_serial_read(&d, (uint8_t *) &hdr, sizeof(struct pb_socket_header));
    virtio_serial_write(&d, &status, 1);
    //LOG_INFO("Got hdr, ep=%lu, sz=%lu",hdr.ep,hdr.sz);
    if (hdr.ep == 2)
    {
        virtio_serial_read(&d, (uint8_t *) &cmd, 
                            sizeof(struct usb_pb_command));

        virtio_serial_write(&d, &status, 1);
        //LOG_INFO("cmd: %lu",cmd.command);
        dev->on_command(dev, &cmd);
    } else {
        LOG_ERR("Unexpected transfer: ep=%lu, sz=%lu", hdr.ep, hdr.sz);
    }
}

uint32_t  plat_usb_transfer (struct usb_device *dev, uint8_t ep, 
                            uint8_t *bfr, uint32_t sz)
{
    uint8_t status = 0;
    UNUSED(dev);

    if (ep & 1)
        LOG_INFO("IN Guest->Host, %2.2X, %u",ep,sz);
    else
        LOG_INFO("OUT Host->Guest, %2.2X, %u",ep,sz);

    if (ep & 1)
    {
        virtio_serial_write(&d, bfr, sz);
    } else { 

        virtio_serial_read(&d, bfr, sz);    
        virtio_serial_write(&d, &status, 1);
    }

    return PB_OK;
}

void      plat_usb_set_address(struct usb_device *dev, uint32_t addr)
{
    UNUSED(dev);
    UNUSED(addr);
}

void      plat_usb_set_configuration(struct usb_device *dev)
{
    UNUSED(dev);
}

void      plat_usb_wait_for_ep_completion(uint32_t ep)
{
    UNUSED(ep);
}

