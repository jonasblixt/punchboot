#ifndef JIFFY_PARTS_H
#define JIFFY_PARTS_H

/* eMMC BOOT0 */
#define UUID_9eef7544_bf68_4bf7_8678_da117cbccba8 \
    (const unsigned char *)"\x9e\xef\x75\x44\xbf\x68\x4b\xf7\x86\x78\xda\x11\x7c\xbc\xcb\xa8"
#define PART_boot0 UUID_9eef7544_bf68_4bf7_8678_da117cbccba8
/* eMMC BOOT1 */
#define UUID_4ee31690_0c9b_4d56_a6a6_e6d6ecfd4d54 \
    (const unsigned char *)"\x4e\xe3\x16\x90\x0c\x9b\x4d\x56\xa6\xa6\xe6\xd6\xec\xfd\x4d\x54"
#define PART_boot1 UUID_4ee31690_0c9b_4d56_a6a6_e6d6ecfd4d54
/* eMMC USER */
#define UUID_1aad85a9_75cd_426d_8dc4_e9bdfeeb6875 \
    (const unsigned char *)"\x1a\xad\x85\xa9\x75\xcd\x42\x6d\x8d\xc4\xe9\xbd\xfe\xeb\x68\x75"
#define PART_user UUID_1aad85a9_75cd_426d_8dc4_e9bdfeeb6875
/* eMMC RPMB */
#define UUID_8d75d8b9_b169_4de6_bee0_48abdc95c408 \
    (const unsigned char *)"\x8d\x75\xd8\xb9\xb1\x69\x4d\xe6\xbe\xe0\x48\xab\xdc\x95\xc4\x08"
#define PART_rpmb UUID_8d75d8b9_b169_4de6_bee0_48abdc95c408

#define UUID_2af755d8_8de5_45d5_a862_014cfa735ce0 \
    (const unsigned char *)"\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#define PART_sys_a UUID_2af755d8_8de5_45d5_a862_014cfa735ce0

#define UUID_c046ccd8_0f2e_4036_984d_76c14dc73992 \
    (const unsigned char *)"\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#define PART_sys_b UUID_c046ccd8_0f2e_4036_984d_76c14dc73992

#define UUID_c284387a_3377_4c0f_b5db_1bcbcff1ba1a \
    (const unsigned char *)"\xc2\x84\x38\x7a\x33\x77\x4c\x0f\xb5\xdb\x1b\xcb\xcf\xf1\xba\x1a"
#define UUID_ac6a1b62_7bd0_460b_9e6a_9a7831ccbfbb \
    (const unsigned char *)"\xac\x6a\x1b\x62\x7b\xd0\x46\x0b\x9e\x6a\x9a\x78\x31\xcc\xbf\xbb"
#define UUID_4581af22_99e6_4a94_b821_b60c42d74758 \
    (const unsigned char *)"\x45\x81\xaf\x22\x99\xe6\x4a\x94\xb8\x21\xb6\x0c\x42\xd7\x47\x58"
#define UUID_da2ca04f_a693_4284_b897_3906cfa1eb13 \
    (const unsigned char *)"\xda\x2c\xa0\x4f\xa6\x93\x42\x84\xb8\x97\x39\x06\xcf\xa1\xeb\x13"
#define UUID_23477731_7e33_403b_b836_899a0b1d55db \
    (const unsigned char *)"\x23\x47\x77\x31\x7e\x33\x40\x3b\xb8\x36\x89\x9a\x0b\x1d\x55\xdb"
#define UUID_6ffd077c_32df_49e7_b11e_845449bd8edd \
    (const unsigned char *)"\x6f\xfd\x07\x7c\x32\xdf\x49\xe7\xb1\x1e\x84\x54\x49\xbd\x8e\xdd"
#define UUID_9697399d_e2da_47d9_8eb5_88daea46da1b \
    (const unsigned char *)"\x96\x97\x39\x9d\xe2\xda\x47\xd9\x8e\xb5\x88\xda\xea\x46\xda\x1b"
#define UUID_c5b8b41c_0fb5_494d_8b0e_eba400e075fa \
    (const unsigned char *)"\xc5\xb8\xb4\x1c\x0f\xb5\x49\x4d\x8b\x0e\xeb\xa4\x00\xe0\x75\xfa"
#define UUID_39792364_d3e3_4013_ac51_caaea65e4334 \
    (const unsigned char *)"\x39\x79\x23\x64\xd3\xe3\x40\x13\xac\x51\xca\xae\xa6\x5e\x43\x34"

/* PB Primary boot state partition */
#define UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e \
    (const unsigned char *)"\xf5\xf8\xc9\xae\xef\xb5\x40\x71\x9b\xa9\xd3\x13\xb0\x82\x28\x1e"
#define PART_primary_state UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e
/* PB Backup boot state partition */
#define UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee \
    (const unsigned char *)"\x65\x6a\xb3\xfc\x58\x56\x4a\x5e\xa2\xae\x5a\x01\x83\x13\xb3\xee"
#define PART_backup_state UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee

#endif
