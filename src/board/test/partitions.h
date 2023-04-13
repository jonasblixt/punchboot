#ifndef BOARD_TEST_PARTITIONS_H
#define BOARD_TEST_PARTITIONS_H

/* virtio disk */
#define UUID_1eacedf3_3790_48c7_8ed8_9188ff49672b (const unsigned char *) "\x1e\xac\xed\xf3\x37\x90\x48\xc7\x8e\xd8\x91\x88\xff\x49\x67\x2b"

#define UUID_2af755d8_8de5_45d5_a862_014cfa735ce0 (const unsigned char *) "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define PART_sys_a UUID_2af755d8_8de5_45d5_a862_014cfa735ce0

#define UUID_c046ccd8_0f2e_4036_984d_76c14dc73992 (const unsigned char *) "\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define PART_sys_b UUID_c046ccd8_0f2e_4036_984d_76c14dc73992

#define UUID_c284387a_3377_4c0f_b5db_1bcbcff1ba1a (const unsigned char *) "\xc2\x84\x38\x7a\x33\x77\x4c\x0f\xb5\xdb\x1b\xcb\xcf\xf1\xba\x1a"
#define UUID_ac6a1b62_7bd0_460b_9e6a_9a7831ccbfbb (const unsigned char *) "\xac\x6a\x1b\x62\x7b\xd0\x46\x0b\x9e\x6a\x9a\x78\x31\xcc\xbf\xbb"

#define UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e (const unsigned char *) "\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
#define PART_primary_state UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e

#define UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee (const unsigned char *) "\x65\x6a\xb3\xfc\x58\x56\x4a\x5e\xa2\xae\x5a\x01\x83\x13\xb3\xee"
#define PART_backup_state UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee

#define UUID_44acdcbe_dcb0_4d89_b0ad_8f96967f8c95 (const unsigned char *) "\x44\xac\xdc\xbe\xdc\xb0\x4d\x89\xb0\xad\x8f\x96\x96\x7f\x8c\x95"
#define UUID_ff4ddc6c_ad7a_47e8_8773_6729392dd1b5 (const unsigned char *) "\xff\x4d\xdc\x6c\xad\x7a\x47\xe8\x87\x73\x67\x29\x39\x2d\xd1\xb5"
#endif
