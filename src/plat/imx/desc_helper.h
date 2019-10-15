/* SPDX-License-Identifier: BSD-2-Clause */
/**
 * @copyright 2018 NXP
 *
 * @file    desc_interface.h
 *
 * @brief   CAAM Descriptor interface.
 */
#ifndef PLAT_IMX_DESC_HELPER_H_
#define PLAT_IMX_DESC_HELPER_H_

#include <stdint.h>

/* Local includes */
#include <plat/imx/desc_defines.h>

/**
 * @brief   Descriptor Entry type
 */
typedef uint32_t descEntry_t;

/**
 * @brief   Descriptor pointer type
 */
typedef uint32_t *descPointer_t;

/**
 * @brief   Descriptor status type
 */
typedef uint32_t descStatus_t;

/**
 * @brief  Returns the number of entries of the descriptor \a desc
 */
#define DESC_NBENTRIES(desc)    GET_JD_DESCLEN(*(descEntry_t *)desc)

/**
 * @brief  Returns the descriptor size in bytes of \a nbEntries
 */
#define DESC_SZBYTES(nbEntries)    (nbEntries * sizeof(descEntry_t))

/**
 * @brief  Descriptor Header starting at index \a idx w/o descriptor length
 */
#define DESC_HDR(idx) \
            (CMD_HDR_JD_TYPE | HDR_JD_ONE | HDR_JD_START_IDX(idx))

/**
 * @brief  Descriptor Header starting at index 0 with descriptor length \a len
 */
#define DESC_HEADER(len) \
            (DESC_HDR(0) | HDR_JD_DESCLEN(len))

/**
 * @brief  Descriptor Header starting at index \a idx
 *         with descriptor length \a len
 */
#define DESC_HEADER_IDX(len, idx) \
            (DESC_HDR(idx) | HDR_JD_DESCLEN(len))
/**
 * @brief  Jump Local of class \a cla to descriptor offset \a offset
 *          if test \a test meet the condition \a cond
 */
#define JUMP_LOCAL(cla, test, cond, offset) \
        (CMD_JUMP_TYPE | CMD_CLASS(cla) | JUMP_TYPE(LOCAL) |    \
        JUMP_TST_TYPE(test) | cond | JMP_LOCAL_OFFSET(offset))
/**
 * @brief  Jump Local of class 1 to descriptor offset \a offset
 *          if test \a test meet the condition \a cond
 */
#define JUMP_C1_LOCAL(test, cond, offset) \
            JUMP_LOCAL(CLASS_1, test, cond, offset)

/**
 * @brief  Load Immediate value of length \a len to register \a dst of
 *         class \a cla
 */
#define LD_IMM(cla, dst, len) \
            (CMD_LOAD_TYPE | CMD_CLASS(cla) | CMD_IMM |    \
            LOAD_DST(dst) | LOAD_LENGTH(len))

/**
 * @brief  Load Immediate value of length \a len to register \a dst w/o class
 */
#define LD_NOCLASS_IMM(dst, len) \
            LD_IMM(CLASS_NO, dst, len)

/**
 * @brief  Load value of length \a len to register \a dst of
 *         class \a cla
 */
#define LD_NOIMM(cla, dst, len) \
            (CMD_LOAD_TYPE | CMD_CLASS(cla) | LOAD_DST(dst) | \
            LOAD_LENGTH(len))

/**
 * @brief  Load value of length \a len to register \a dst of
 *         class \a cla starting of register offset \a off
 */
#define LD_NOIMM_OFF(cla, dst, len, off) \
            (CMD_LOAD_TYPE | CMD_CLASS(cla) | LOAD_DST(dst) | \
            LOAD_OFFSET(off) | LOAD_LENGTH(len))

/**
 * @brief  FIFO Load to register \a dst class \a cla with action \a act.
 *
 */
#define FIFO_LD(cla, dst, act, len) \
            (CMD_FIFO_LOAD_TYPE | CMD_CLASS(cla) |        \
            FIFO_LOAD_INPUT(dst) | FIFO_LOAD_ACTION(act) |    \
            FIFO_LOAD_LENGTH(len))

/**
 * @brief  FIFO Load to register \a dst class \a cla with action \a act.
 *         Pointer is a Scatter/Gatter Table
 *
 */
#define FIFO_LD_SGT(cla, dst, act, len) \
            (CMD_FIFO_LOAD_TYPE | CMD_CLASS(cla) | CMD_SGT | \
            FIFO_LOAD_INPUT(dst) | FIFO_LOAD_ACTION(act) |   \
            FIFO_LOAD_LENGTH(len))

/**
 * @brief  FIFO Load to register \a dst class \a cla with action \a act. \n
 *         The length is externally defined
 *
 */
#define FIFO_LD_EXT(cla, dst, act) \
            (CMD_FIFO_LOAD_TYPE | FIFO_LOAD_EXT |    \
            CMD_CLASS(cla) | FIFO_LOAD_INPUT(dst) |    \
            FIFO_LOAD_ACTION(act))

/**
 * @brief  FIFO Load Immediate data length \a len to register \a dst
 *         class \a cla with action \a act.
 *
 */
#define FIFO_LD_IMM(cla, dst, act, len) \
            (CMD_FIFO_LOAD_TYPE | CMD_IMM |            \
            CMD_CLASS(cla) | FIFO_LOAD_INPUT(dst) |        \
            FIFO_LOAD_ACTION(act) | FIFO_LOAD_LENGTH(len))

/**
 * @brief  Store value of length \a len from register \a src of
 *         class \a cla
 */
#define ST_NOIMM(cla, src, len) \
            (CMD_STORE_TYPE | CMD_CLASS(cla) | STORE_SRC(src) | \
            STORE_LENGTH(len))

/**
 * @brief  Store value of length \a len from register \a src of
 *         class \a cla starting at register offset \a off
 */
#define ST_NOIMM_OFF(cla, src, len, off) \
            (CMD_STORE_TYPE | CMD_CLASS(cla) | STORE_SRC(src) | \
            STORE_OFFSET(off) | STORE_LENGTH(len))

/**
 * @brief  FIFO Store from register \a src of length \a len
 */
#define FIFO_ST(src, len) \
            (CMD_FIFO_STORE_TYPE | FIFO_STORE_OUTPUT(src) | \
            FIFO_STORE_LENGTH(len))

/**
 * @brief  FIFO Store from register \a src. \n
 *         The length is externally defined
 */
#define FIFO_ST_EXT(src) \
            (CMD_FIFO_STORE_TYPE | FIFO_LOAD_EXT | \
             FIFO_STORE_OUTPUT(src))

/**
 * @brief  FIFO Store from register \a src of length \a len. Pointer is
 *         a Scatter/Gatter Table
 */
#define FIFO_ST_SGT(src, len) \
            (CMD_FIFO_STORE_TYPE | CMD_SGT | \
             FIFO_STORE_OUTPUT(src) | FIFO_STORE_LENGTH(len))

/**
 * @brief  RNG State Handle instantation operation for \a sh id
 */
#define RNG_SH_INST(sh) \
            (CMD_OP_TYPE | OP_TYPE(CLASS1) | OP_ALGO(RNG) | \
            ALGO_RNG_SH(sh) | ALGO_AS(RNG_INSTANTIATE))

/**
 * @brief  RNG Generates Secure Keys
 */
#define RNG_GEN_SECKEYS \
            (CMD_OP_TYPE | OP_TYPE(CLASS1) | OP_ALGO(RNG) | \
            ALGO_RNG_SK | ALGO_AS(RNG_GENERATE))

/**
 * @brief  RNG Generates Data
 */
#define RNG_GEN_DATA \
            (CMD_OP_TYPE | OP_TYPE(CLASS1) | OP_ALGO(RNG) | \
             ALGO_AS(RNG_GENERATE))

/**
 * @brief  HASH Init Operation of algorithm \a algo
 */
#define HASH_INIT(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(INIT) | ALGO_ENCRYPT)

/**
 * @brief  HASH Update Operation of algorithm \a algo
 */
#define HASH_UPDATE(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(UPDATE) | ALGO_ENCRYPT)

/**
 * @brief  HASH Final Operation of algorithm \a algo
 */
#define HASH_FINAL(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(FINAL) | ALGO_ENCRYPT)

/**
 * @brief  HASH Init and Final Operation of algorithm \a algo
 */
#define HASH_INITFINAL(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(INIT_FINAL) | ALGO_ENCRYPT)

/**
 * @brief  HMAC Init Decryption Operation of algorithm \a algo
 */
#define HMAC_INIT_DECRYPT(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(INIT) | ALGO_AAI(DIGEST_HMAC) | ALGO_DECRYPT)

/**
 * @brief  HMAC Init Operation of algorithm \a algo with Precomp key
 */
#define HMAC_INITFINAL_PRECOMP(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(INIT_FINAL) | ALGO_AAI(DIGEST_HMAC_PRECOMP) | \
            ALGO_ENCRYPT)

/**
 * @brief  HMAC Init and Final Operation of algorithm \a algo with Precomp key
 */
#define HMAC_INIT_PRECOMP(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(INIT) | ALGO_AAI(DIGEST_HMAC_PRECOMP) | \
            ALGO_ENCRYPT)
/**
 * @brief  HMAC Final Operation of algorithm \a algo with Precomp key
 */
#define HMAC_FINAL_PRECOMP(algo) \
            (CMD_OP_TYPE | OP_TYPE(CLASS2) | (algo) | \
            ALGO_AS(FINAL) | ALGO_AAI(DIGEST_HMAC_PRECOMP) | \
            ALGO_ENCRYPT)

/**
 * @brief  Cipher Init and Final Operation of algorithm \a algo
 */
#define CIPHER_INITFINAL(algo, encrypt) \
            (CMD_OP_TYPE | OP_TYPE(CLASS1) | (algo) | \
             ALGO_AS(INIT_FINAL) | \
            ((encrypt == true) ? ALGO_ENCRYPT : ALGO_DECRYPT))

/**
 * @brief  Cipher Init Operation of algorithm \a algo
 */
#define CIPHER_INIT(algo, encrypt) \
            (CMD_OP_TYPE | OP_TYPE(CLASS1) | (algo) | \
             ALGO_AS(INIT) | \
            ((encrypt == true) ? ALGO_ENCRYPT : ALGO_DECRYPT))

/**
 * @brief  Cipher Update Operation of algorithm \a algo
 */
#define CIPHER_UPDATE(algo, encrypt) \
            (CMD_OP_TYPE | OP_TYPE(CLASS1) | (algo) | \
             ALGO_AS(UPDATE) | \
            ((encrypt == true) ? ALGO_ENCRYPT : ALGO_DECRYPT))

/**
 * @brief  Cipher Final Operation of algorithm \a algo
 */
#define CIPHER_FINAL(algo, encrypt) \
            (CMD_OP_TYPE | OP_TYPE(CLASS1) | (algo) | \
             ALGO_AS(FINAL) | \
            ((encrypt == true) ? ALGO_ENCRYPT : ALGO_DECRYPT))

/**
 * @brief   Load a class \a cla key of length \a len to register \a dst.
 *          Key can be store in plain text.
 */
#define LD_KEY_PLAIN(cla, dst, len) \
            (CMD_KEY_TYPE | CMD_CLASS(cla) | KEY_PTS | \
            KEY_DEST(dst) | KEY_LENGTH(len))

/**
 * @brief   Load a split key of length \a len.
 */
#define LD_KEY_SPLIT(len) \
            (CMD_KEY_TYPE | CMD_CLASS(CLASS_2) | \
            KEY_DEST(MDHA_SPLIT) | \
            KEY_LENGTH(len))

/**
 * @brief  MPPRIVK generation function.
 */
#define MPPRIVK \
            (CMD_OP_TYPE | OP_TYPE(ENCAPS) | PROTID(MPKEY))

/**
 * @brief  MPPUBK generation function.
 */
#define MPPUBK \
            (CMD_OP_TYPE | OP_TYPE(DECAPS) | PROTID(MPKEY))

/**
 * @brief  MPSIGN function.
 */
#define MPSIGN_OP \
            (CMD_OP_TYPE | OP_TYPE(DECAPS) | PROTID(MPSIGN))

#endif  // PLAT_IMX_DESC_HELPER_H_

