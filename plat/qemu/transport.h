#ifndef PLAT_QEMU_TRANSPORT_H_
#define PLAT_QEMU_TRANSPORT_H_

#include <pb/pb.h>
#include <pb/transport.h>

int virtio_serial_transport_setup(struct pb_transport *transport,
                                  struct pb_transport_driver *drv);

#endif  // PLAT_QEMU_TRANSPORT_H_
