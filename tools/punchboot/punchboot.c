#include <stdio.h>
#include <libusb-1.0/libusb.h>



static libusb_device * find_device(libusb_device **devs)
{
	libusb_device *dev;
	int i = 0;

	while ((dev = devs[i++]) != NULL) {
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0) {
			return NULL;
		}

        if ( (desc.idVendor == 0xFFFF) && (desc.idProduct == 0x0001)) {
            return dev;
        }
		
	}

    return NULL;
}



/*
 * 
 *
 *
 * */

int main(void)
{
	libusb_device **devs;
    libusb_device *dev;
	int r;
	ssize_t cnt;

	r = libusb_init(NULL);
	if (r < 0)
		return r;

	cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int) cnt;

	dev = find_device(devs);
	libusb_free_device_list(devs, 1);

    if (dev == NULL) {
        printf ("Could not find device\n\r");
        libusb_exit (NULL);
        return -1;
    }

	libusb_exit(NULL);
	return 0;
}
