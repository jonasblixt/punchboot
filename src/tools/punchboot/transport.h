#ifndef __PUNCHBOOT_USB_H__
#define __PUNCHBOOT_USB_H__

#include <stdint.h>

int transport_init(uint8_t *usb_path, uint8_t usb_path_count);
int pb_write(uint32_t cmd, uint32_t arg0,
                           uint32_t arg1,
                           uint32_t arg2,
                           uint32_t arg3,
                           uint8_t *bfr, int sz);
int pb_read(uint8_t *bfr, int sz);
int pb_write_bulk(uint8_t *bfr, int sz, int *sz_tx);
void transport_exit(void);
uint32_t pb_read_result_code(void);

#endif
