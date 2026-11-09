#ifndef IMXRT_PARTS_H
#define IMXRT_PARTS_H

/* NOR Partitions. System A/B and the boot state use the UUIDs of the other
 * boards' GPTs. */
#define UUID_56bac460_95a2_4e63_8174_64261086487d \
    (const unsigned char *)"\x56\xba\xc4\x60\x95\xa2\x4e\x63\x81\x74\x64\x26\x10\x86\x48\x7d"
#define PART_NOR_PBL UUID_56bac460_95a2_4e63_8174_64261086487d

#define UUID_2af755d8_8de5_45d5_a862_014cfa735ce0 \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define PART_sys_a UUID_2af755d8_8de5_45d5_a862_014cfa735ce0

#define UUID_c046ccd8_0f2e_4036_984d_76c14dc73992 \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define PART_sys_b UUID_c046ccd8_0f2e_4036_984d_76c14dc73992

#define UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e \
    (const unsigned char *)"\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
#define PART_primary_state UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e

#define UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee \
    (const unsigned char *)"\x65\x6a\xb3\xfc\x58\x56\x4a\x5e\xa2\xae\x5a\x01\x83\x13\xb3\xee"
#define PART_backup_state UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee

#endif
