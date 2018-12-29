#ifndef __PUNCHBOOT_USB_H__
#define __PUNCHBOOT_USB_H__

#include <stdint.h>

int transport_init(void);
int pb_write(uint32_t cmd, uint8_t *bfr, int sz);
int pb_read(uint8_t *bfr, int sz);
int pb_write_bulk(uint8_t *bfr, int sz, int *sz_tx);
void transport_exit(void);
uint32_t pb_read_result_code(void);

#endif
