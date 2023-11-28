/* SPDX-License-Identifier: BSD-2-Clause */
/**
 * @copyright 2018 NXP
 *
 * @file    desc_defines.h
 *
 * @brief   CAAM Descriptor defines.
 */
#ifndef PLAT_IMX_DESC_DEFINES_H_
#define PLAT_IMX_DESC_DEFINES_H_

#define BIT32(nr)                                 (UINT32_C(1) << (nr))
#define SHIFT_U32(v, shift)                       ((uint32_t)(v) << (shift))

/*
 * Common Command constants
 */
#define CMD_TYPE(cmd)                             SHIFT_U32((cmd & 0x1F), 27)
#define CMD_CLASS(val)                            SHIFT_U32((val & 0x3), 25)
#define CLASS_NO                                  0x0
#define CLASS_1                                   0x1
#define CLASS_2                                   0x2

#define CMD_SGT                                   BIT32(24)
#define CMD_IMM                                   BIT32(23)

/*
 * HEADER Job Descriptor Header format
 */
#define CMD_HDR_JD_TYPE                           CMD_TYPE(0x16)

/* Must be ONE */
#define HDR_JD_ONE                                BIT32(23)

/* Start Index if SHR = 0 */
#define HDR_JD_START_IDX(line)                    SHIFT_U32((line & 0x3F), 16)

/* Descriptor Length */
#define HDR_JD_DESCLEN(len)                       SHIFT_U32((len & 0x7F), 0)
#define GET_JD_DESCLEN(entry)                     (entry & 0x7F)

/*
 * KEY Command fields
 */
#define CMD_KEY_TYPE                              CMD_TYPE(0x00)

/* Key Destination */
#define KEY_DEST(val)                             SHIFT_U32((KEY_DEST_##val & 0x3), 16)
#define KEY_DEST_REG                              0x0
#define KEY_DEST_PKHA_E                           0x1
#define KEY_DEST_AFHA_SBOX                        0x2
#define KEY_DEST_MDHA_SPLIT                       0x3

/* Plaintext Store */
#define KEY_PTS                                   BIT32(14)

/* Key Length */
#define KEY_LENGTH(len)                           SHIFT_U32((len & 0x3FF), 0)

/*
 * LOAD Command fields
 */
#define CMD_LOAD_TYPE                             CMD_TYPE(0x02)

/* Load Destination */
#define LOAD_DST(reg)                             SHIFT_U32((reg & 0x7F), 16)

/* Offset in destination register */
#define LOAD_OFFSET(off)                          SHIFT_U32((off & 0xFF), 8)

/* Length */
#define LOAD_LENGTH(len)                          SHIFT_U32((len & 0xFF), 0)

/*
 * STORE Command fields
 */
#define CMD_STORE_TYPE                            CMD_TYPE(0x0A)

/* Store Source */
#define STORE_SRC(reg)                            SHIFT_U32((reg & 0x7F), 16)

/* Offset in source register */
#define STORE_OFFSET(off)                         SHIFT_U32((off & 0xFF), 8)

/* Length */
#define STORE_LENGTH(len)                         SHIFT_U32((len & 0xFF), 0)

/*
 * Define the Load/Store Registers Source and Destination
 */
#define REG_MODE                                  0x00
#define REG_KEY_SIZE                              0x01
#define REG_DATA_SIZE                             0x02
#define REG_ICV_SIZE                              0x03
#define REG_DECO_MID_STATUS                       0x04
#define REG_DECO_CTRL2                            0x05
#define REG_CHA_CTRL                              0x06
#define REG_DECO_CTRL                             0x06
#define REG_IRQ_CTRL                              0x07
#define REG_DECO_PROT_OVERWRITE                   0x07
#define REG_CLEAR_WRITTEN                         0x08
#define REG_MATH0                                 0x08
#define REG_MATH1                                 0x09
#define REG_MATH2                                 0x0A
#define REG_CHA_INST_SELECT                       0x0A
#define REG_AAD_SIZE                              0x0B
#define REG_MATH3                                 0x0B
#define REG_ALT_DATA_SIZE_C1                      0x0F
#define REG_PKHA_A_SIZE                           0x10
#define REG_PKHA_B_SIZE                           0x11
#define REG_PKHA_N_SIZE                           0x12
#define REG_PKHA_E_SIZE                           0x13
#define REG_CTX                                   0x20
#define REG_MATH0_DW                              0x30
#define REG_MATH1_DW                              0x31
#define REG_MATH2_DW                              0x32
#define REG_MATH3_DW                              0x33
#define REG_MATH0_B                               0x38
#define REG_MATH1_B                               0x39
#define REG_MATH2_B                               0x3A
#define REG_MATH3_B                               0x3B
#define REG_KEY                                   0x40
#define REG_DECO_DESC                             0x40
#define REG_NFIFO_n_SIZE                          0x70
#define REG_NFIFO_MATH                            0x73
#define REG_SIZE                                  0x74
#define REG_SIZE_MATH                             0x75
#define REG_IFIFO_SHIFT                           0x76
#define REG_OFIFO_SHIFT                           0x77
#define REG_AUX_FIFO                              0x78
#define REG_NFIFO                                 0x7A
#define REG_IFIFO                                 0x7C
#define REG_OFIFO                                 0x7E

/*
 * FIFO LOAD Command fields
 */
#define CMD_FIFO_LOAD_TYPE                        CMD_TYPE(0x04)

/* Extended Length */
#define FIFO_LOAD_EXT                             BIT32(22)

/* Input data */
#define FIFO_LOAD_INPUT(reg)                      SHIFT_U32((FIFO_LOAD_##reg & 0x3F), 16)
#define FIFO_LOAD_ACTION(act)                     SHIFT_U32((FIFO_LOAD_##act & 0x3F), 16)

/* Length */
#define FIFO_LOAD_MAX                             0xFFFF
#define FIFO_LOAD_LENGTH(len)                     SHIFT_U32((len & FIFO_LOAD_MAX), 0)

/*
 * Define the FIFO Load Type Input
 */
#define FIFO_LOAD_PKHA_A0                         0x00
#define FIFO_LOAD_PKHA_A1                         0x01
#define FIFO_LOAD_PKHA_A2                         0x02
#define FIFO_LOAD_PKHA_A3                         0x03
#define FIFO_LOAD_PKHA_B0                         0x04
#define FIFO_LOAD_PKHA_B1                         0x05
#define FIFO_LOAD_PKHA_B2                         0x06
#define FIFO_LOAD_PKHA_B3                         0x07
#define FIFO_LOAD_PKHA_N                          0x08
#define FIFO_LOAD_PKHA_A                          0x0C
#define FIFO_LOAD_PKHA_B                          0x0D
#define FIFO_LOAD_NO_INFO_NFIFO                   0x0F
#define FIFO_LOAD_MSG                             0x10
#define FIFO_LOAD_MSG_C1_OUT_C2                   0x18
#define FIFO_LOAD_IV                              0x20
#define FIFO_LOAD_BITDATA                         0x2C
#define FIFO_LOAD_AAD                             0x30
#define FIFO_LOAD_ICV                             0x38

/* Define Action of some FIFO Data */
#define FIFO_LOAD_NOACTION                        0x0
#define FIFO_LOAD_FLUSH                           0x1
#define FIFO_LOAD_LAST_C1                         0x2
#define FIFO_LOAD_LAST_C2                         0x4

/*
 * FIFO STORE Command fields
 */
#define CMD_FIFO_STORE_TYPE                       CMD_TYPE(0x0C)

/* Extended Length */
#define FIFO_STORE_EXT                            BIT32(22)

/* Output data */
#define FIFO_STORE_OUTPUT(reg)                    SHIFT_U32((FIFO_STORE_##reg & 0x3F), 16)

/* Length */
#define FIFO_STORE_MAX                            0xFFFF
#define FIFO_STORE_LENGTH(len)                    SHIFT_U32((len & FIFO_STORE_MAX), 0)

/*
 * Define the FIFO Store Type Output
 */
#define FIFO_STORE_PKHA_A0                        0x00
#define FIFO_STORE_PKHA_A1                        0x01
#define FIFO_STORE_PKHA_A2                        0x02
#define FIFO_STORE_PKHA_A3                        0x03
#define FIFO_STORE_PKHA_B0                        0x04
#define FIFO_STORE_PKHA_B1                        0x05
#define FIFO_STORE_PKHA_B2                        0x06
#define FIFO_STORE_PKHA_B3                        0x07
#define FIFO_STORE_PKHA_N                         0x08
#define FIFO_STORE_PKHA_A                         0x0C
#define FIFO_STORE_PKHA_B                         0x0D
#define FIFO_STORE_AFHA_SBOX_AES_CCM_JKEK         0x10
#define FIFO_STORE_AFHA_SBOX_AES_CCM_TKEK         0x11
#define FIFO_STORE_PKHA_E_AES_CCM_JKEK            0x12
#define FIFO_STORE_PKHA_E_AES_CCM_TKEK            0x13
#define FIFO_STORE_KEY_AES_CCM_JKEK               0x14
#define FIFO_STORE_KEY_AES_CCM_TKEK               0x15
#define FIFO_STORE_C2_MDHA_SPLIT_KEY_AES_CCM_JKEK 0x16
#define FIFO_STORE_C2_MDHA_SPLIT_KEY_AES_CCM_TKEK 0x17
#define FIFO_STORE_AFHA_SBOX_AES_ECB_JKEK         0x20
#define FIFO_STORE_AFHA_SBOX_AES_ECB_TKEK         0x21
#define FIFO_STORE_PKHA_E_AES_ECB_JKEK            0x22
#define FIFO_STORE_PKHA_E_AES_ECB_TKEK            0x23
#define FIFO_STORE_KEY_AES_ECB_JKEK               0x24
#define FIFO_STORE_KEY_AES_ECB_TKEK               0x25
#define FIFO_STORE_C2_MDHA_SPLIT_KEY_AES_ECB_JKEK 0x26
#define FIFO_STORE_C2_MDHA_SPLIT_KEY_AES_ECB_TKEK 0x27
#define FIFO_STORE_MSG_DATA                       0x30
#define FIFO_STORE_RNG_TO_MEM                     0x34
#define FIFO_STORE_RNG_STAY_FIFO                  0x35
#define FIFO_STORE_SKIP                           0x3F

/*
 * Operation Command fields
 * Algorithm/Protocol/PKHA
 */
#define CMD_OP_TYPE                               CMD_TYPE(0x10)

/* Operation Type */
#define OP_TYPE(type)                             SHIFT_U32((OP_TYPE_##type & 0x7), 24)
#define OP_TYPE_UNI                               0x0
#define OP_TYPE_PKHA                              0x1
#define OP_TYPE_CLASS1                            0x2
#define OP_TYPE_CLASS2                            0x4
#define OP_TYPE_DECAPS                            0x6
#define OP_TYPE_ENCAPS                            0x7

/* Protocol Identifier */
#define PROTID(id)                                SHIFT_U32((PROTID_##id & 0xFF), 16)
#define PROTID_MPKEY                              0x14
#define PROTID_MPSIGN                             0x15

/*
 * Algorithm Identifier
 */
#define OP_ALGO(algo)                             SHIFT_U32((ALGO_##algo & 0xFF), 16)
#define ALGO_AES                                  0x10
#define ALGO_DES                                  0x20
#define ALGO_3DES                                 0x21
#define ALGO_ARC4                                 0x30
#define ALGO_RNG                                  0x50
#define ALGO_MD5                                  0x40
#define ALGO_SHA1                                 0x41
#define ALGO_SHA224                               0x42
#define ALGO_SHA256                               0x43
#define ALGO_SHA384                               0x44
#define ALGO_SHA512                               0x45
#define ALGO_SHA512_224                           0x46
#define ALGO_SHA512_256                           0x47

/* Algorithm Additional Information */
#define ALGO_AAI(info)                            SHIFT_U32((AAI_##info & 0x1FF), 4)

// AES AAI
#define AAI_AES_CTR_MOD128                        0x00
#define AAI_AES_CBC                               0x10
#define AAI_AES_ECB                               0x20
#define AAI_AES_CFB                               0x30
#define AAI_AES_OFB                               0x40
#define AAI_AES_CMAC                              0x60
#define AAI_AES_XCBC_MAC                          0x70
#define AAI_AES_CCM                               0x80
#define AAI_AES_GCM                               0x90

// DES AAI
#define AAI_DES_CBC                               0x10
#define AAI_DES_ECB                               0x20
#define AAI_DES_CFB                               0x30
#define AAI_DES_OFB                               0x40

// Digest MD5/SHA AAI
#define AAI_DIGEST_HASH                           0x00
#define AAI_DIGEST_HMAC                           0x01
#define AAI_DIGEST_SMAC                           0x02
#define AAI_DIGEST_HMAC_PRECOMP                   0x04

/* Algorithm State */
#define ALGO_AS(state)                            SHIFT_U32((AS_##state & 0x3), 2)
#define AS_UPDATE                                 0x0
#define AS_INIT                                   0x1
#define AS_FINAL                                  0x2
#define AS_INIT_FINAL                             0x3

/* Algorithm Encrypt/Decrypt */
#define ALGO_DECRYPT                              SHIFT_U32(0x0, 0)
#define ALGO_ENCRYPT                              SHIFT_U32(0x1, 0)

/*
 * Specific RNG Algorithm bits 12-0
 */
/* Secure Key */
#define ALGO_RNG_SK                               BIT32(12)

/* State Handle */
#define ALGO_RNG_SH(sh)                           SHIFT_U32((sh & 0x3), 4)

/* State */
#define AS_RNG_GENERATE                           0x0
#define AS_RNG_INSTANTIATE                        0x1
#define AS_RNG_RESEED                             0x2
#define AS_RNG_UNINSTANTIATE                      0x3

/*
 * JUMP Command fields
 */
#define CMD_JUMP_TYPE                             CMD_TYPE(0x14)

/* Jump Type */
#define JUMP_TYPE(type)                           SHIFT_U32((JMP_##type & 0xF), 20)
#define JMP_LOCAL                                 0x0
#define JMP_LOCAL_INC                             0x1
#define JMP_SUBROUTINE_CALL                       0x2
#define JMP_LOCAL_DEC                             0x3
#define JMP_NON_LOCAL                             0x4
#define JMP_SUBROUTINE_RET                        0x6
#define JMP_HALT                                  0x8
#define JMP_HALT_USER_STATUS                      0xC

/* Test Type */
#define JUMP_TST_TYPE(type)                       SHIFT_U32((JMP_##type & 0x3), 16)
#define JMP_TST_ALL_COND_TRUE                     0x0
#define JMP_TST_ALL_COND_FALSE                    0x1
#define JMP_TST_ANY_COND_TRUE                     0x2
#define JMP_TST_ANY_COND_FALSE                    0x3

/* Test Condition */
#define JUMP_TST_COND(cond)                       SHIFT_U32((JMP_COND_##cond & 0xFF), 8)
#define JMP_COND_NONE                             0x00
#define JMP_COND_PKHA_IS_ZERO                     0x80
#define JMP_COND_PKHA_GCD_1                       0x40
#define JMP_COND_PKHA_IS_PRIME                    0x20
#define JMP_COND_MATH_N                           0x08
#define JMP_COND_MATH_Z                           0x04
#define JMP_COND_MATH_C                           0x02
#define JMP_COND_MATH_NV                          0x01

/* Local Offset */
#define JMP_LOCAL_OFFSET(off)                     SHIFT_U32((off & 0xFF), 0)

/*
 * Protocol Data Block
 */
#define PDB_MP_CSEL_P256                          0x03
#define PDB_MP_CSEL_P384                          0x04
#define PDB_MP_CSEL_P521                          0x05

#endif // PLAT_IMX_DESC_DEFINES_H_
