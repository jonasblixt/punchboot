#ifndef __PB_BOOT_H__
#define __PB_BOOT_H__

#include <stdint.h>
#include <image.h>
#include <gpt.h>

void pb_boot_linux_with_dt(struct pb_pbi *pbi);

#endif
