#ifndef IMX8MPEVK_PARTS_H
#define IMX8MPEVK_PARTS_H

/* The eMMC, System A/B and the boot state */
#include <pb/board_layout.h>

/* SD card (uSDHC2), the whole card */
#define UUID_a73fd8a4_0279_4ae0_ad8d_0bba4be03468 \
    (const unsigned char *)"\xa7\x3f\xd8\xa4\x02\x79\x4a\xe0\xad\x8d\x0b\xba\x4b\xe0\x34\x68"
#define PART_sd UUID_a73fd8a4_0279_4ae0_ad8d_0bba4be03468

#endif
