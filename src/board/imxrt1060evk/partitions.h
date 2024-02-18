#ifndef IMXRT_PARTS_H
#define IMXRT_PARTS_H

/* NOR Partitions */
#define UUID_56bac460_95a2_4e63_8174_64261086487d \
    (const unsigned char *)"\x56\xba\xc4\x60\x95\xa2\x4e\x63\x81\x74\x64\x26\x10\x86\x48\x7d"
#define PART_NOR_PBL UUID_56bac460_95a2_4e63_8174_64261086487d

#define UUID_4d45fafd_bff0_451d_ae01_fe487145988e \
    (const unsigned char *)"\x4d\x45\xfa\xfd\xbf\xf0\x45\x1d\xae\x01\xfe\x48\x71\x45\x98\x8e"
#define PART_NOR_SBL1 UUID_4d45fafd_bff0_451d_ae01_fe487145988e

#define UUID_a47a8b2d_e91d_47f4_9cac_48fb045e983e \
    (const unsigned char *)"\xa4\x7a\x8b\x2d\xe9\x1d\x47\xf4\x9c\xac\x48\xfb\x04\x5e\x98\x3e"
#define PART_NOR_config UUID_a47a8b2d_e91d_47f4_9cac_48fb045e983e

#define UUID_771998a8_c25e_414b_ae23_8795c10b8188 \
    (const unsigned char *)"\x77\x19\x98\xa8\xc2\x5e\x41\x4b\xae\x23\x87\x95\xc1\x0b\x81\x88"
#define PART_NOR_application UUID_771998a8_c25e_414b_ae23_8795c10b8188

#define UUID_081a7bb7_f265_4259_a2f8_f710a9c6f074 \
    (const unsigned char *)"\x08\x1a\x7b\xb7\xf2\x65\x42\x59\xa2\xf8\xf7\x10\xa9\xc6\xf0\x74"
#define PART_NOR_SBL2 UUID_081a7bb7_f265_4259_a2f8_f710a9c6f074

#endif
