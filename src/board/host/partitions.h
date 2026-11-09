#ifndef BOARD_HOST_PARTITIONS_H
#define BOARD_HOST_PARTITIONS_H

/* System A/B and the boot state */
#include <pb/board_layout.h>

/* The host disk */
#define UUID_1eacedf3_3790_48c7_8ed8_9188ff49672b \
    (const unsigned char *)"\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"
#define PART_disk UUID_1eacedf3_3790_48c7_8ed8_9188ff49672b

#define UUID_44acdcbe_dcb0_4d89_b0ad_8f96967f8c95 \
    (const unsigned char *)"\x44\xac\xdc\xbe\xdc\xb0\x4d\x89\xb0\xad\x8f\x96\x96\x7f\x8c\x95"
#define PART_fusebox UUID_44acdcbe_dcb0_4d89_b0ad_8f96967f8c95

#define UUID_ff4ddc6c_ad7a_47e8_8773_6729392dd1b5 \
    (const unsigned char *)"\xff\x4d\xdc\x6c\xad\x7a\x47\xe8\x87\x73\x67\x29\x39\x2d\xd1\xb5"
#define PART_readable UUID_ff4ddc6c_ad7a_47e8_8773_6729392dd1b5
#endif
