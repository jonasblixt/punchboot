#ifndef IMXRT_PARTS_H
#define IMXRT_PARTS_H

/* System A/B and the boot state: the UUIDs of the other boards' GPTs */
#include <pb/board_layout.h>

/* NOR Partitions */
#define UUID_56bac460_95a2_4e63_8174_64261086487d \
    (const unsigned char *)"\x56\xba\xc4\x60\x95\xa2\x4e\x63\x81\x74\x64\x26\x10\x86\x48\x7d"
#define PART_NOR_PBL UUID_56bac460_95a2_4e63_8174_64261086487d

#endif
