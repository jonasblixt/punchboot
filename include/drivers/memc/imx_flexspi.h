/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 * Copyright (C) 2023 Marten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_MEMC_IMX_FLEXSPI_H
#define INCLUDE_DRIVERS_MEMC_IMX_FLEXSPI_H

#include <stdint.h>

#define FLEXSPI_CR1_CS_INTERVAL(x)   ((x & 0xffff) << 16)
#define FLEXSPI_CR1_CS_INTERVAL_UNIT (1 << 15)
#define FLEXSPI_CR1_CAS(x)           ((x & 0x0f) << 11)
#define FLEXSPI_CR1_WA               (1 << 10)
#define FLEXSPI_CR1_TCSH(x)          ((x & 0x1F) << 5)
#define FLEXSPI_CR1_TCSS(x)          (x & 0x1F)

#define FLEXSPI_CR2_ARDSEQNUM(x)     ((x & 0x07) << 5)
#define FLEXSPI_CR2_ARDSEQID(x)      (x & 0x0f)

#define FLEXSPI_CR4_WMOPT1           BIT(0)

#define FLEXSPI_DLLCR_OVRDEN         BIT(8)

#define FLEXSPI_1PAD                 0
#define FLEXSPI_2PAD                 1
#define FLEXSPI_4PAD                 2
#define FLEXSPI_8PAD                 3

#define FLEXSPI_LUT_OPERAND0_MASK    (0xffu)
#define FLEXSPI_LUT_OPERAND0_SHIFT   (0U)
#define FLEXSPI_LUT_OPERAND0(x) \
    (((uint32_t)(((uint32_t)(x)) << FLEXSPI_LUT_OPERAND0_SHIFT)) & FLEXSPI_LUT_OPERAND0_MASK)
#define FLEXSPI_LUT_NUM_PADS0_MASK  (0x300u)
#define FLEXSPI_LUT_NUM_PADS0_SHIFT (8u)
#define FLEXSPI_LUT_NUM_PADS0(x) \
    (((uint32_t)(((uint32_t)(x)) << FLEXSPI_LUT_NUM_PADS0_SHIFT)) & FLEXSPI_LUT_NUM_PADS0_MASK)
#define FLEXSPI_LUT_OPCODE0_MASK  (0xfc00u)
#define FLEXSPI_LUT_OPCODE0_SHIFT (10u)
#define FLEXSPI_LUT_OPCODE0(x) \
    (((uint32_t)(((uint32_t)(x)) << FLEXSPI_LUT_OPCODE0_SHIFT)) & FLEXSPI_LUT_OPCODE0_MASK)
#define FLEXSPI_LUT_OPERAND1_MASK  (0xff0000u)
#define FLEXSPI_LUT_OPERAND1_SHIFT (16U)
#define FLEXSPI_LUT_OPERAND1(x) \
    (((uint32_t)(((uint32_t)(x)) << FLEXSPI_LUT_OPERAND1_SHIFT)) & FLEXSPI_LUT_OPERAND1_MASK)
#define FLEXSPI_LUT_NUM_PADS1_MASK  (0x3000000u)
#define FLEXSPI_LUT_NUM_PADS1_SHIFT (24u)
#define FLEXSPI_LUT_NUM_PADS1(x) \
    (((uint32_t)(((uint32_t)(x)) << FLEXSPI_LUT_NUM_PADS1_SHIFT)) & FLEXSPI_LUT_NUM_PADS1_MASK)
#define FLEXSPI_LUT_OPCODE1_MASK  (0xfc000000u)
#define FLEXSPI_LUT_OPCODE1_SHIFT (26u)
#define FLEXSPI_LUT_OPCODE1(x) \
    (((uint32_t)(((uint32_t)(x)) << FLEXSPI_LUT_OPCODE1_SHIFT)) & FLEXSPI_LUT_OPCODE1_MASK)

#define FLEXSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1)                                  \
    (FLEXSPI_LUT_OPERAND0(op0) | FLEXSPI_LUT_NUM_PADS0(pad0) | FLEXSPI_LUT_OPCODE0(cmd0) | \
     FLEXSPI_LUT_OPERAND1(op1) | FLEXSPI_LUT_NUM_PADS1(pad1) | FLEXSPI_LUT_OPCODE1(cmd1))

enum flexspi_port {
    FLEXSPI_PORT_A1 = 0x0U, /*!< Access flash on A1 port. */
    FLEXSPI_PORT_A2, /*!< Access flash on A2 port. */
    FLEXSPI_PORT_B1, /*!< Access flash on B1 port. */
    FLEXSPI_PORT_B2, /*!< Access flash on B2 port. */
    FLEXSPI_PORT_COUNT
};

enum flexspi_command_type {
    FLEXSPI_COMMAND, /*!< FlexSPI operation: Only command, both TX and Rx buffer are ignored. */
    FLEXSPI_CONFIG, /*!< FlexSPI operation: Configure device mode, the TX fifo size is fixed in
                        LUT. */
    FLEXSPI_READ, /* /!< FlexSPI operation: Read, only Rx Buffer is effective. */
    FLEXSPI_WRITE, /* /!< FlexSPI operation: Read, only Tx Buffer is effective. */
};

enum flexspi_command {
    FLEXSPI_COMMAND_STOP = 0x00U, /*!< Stop execution, deassert CS. */
    FLEXSPI_COMMAND_SDR = 0x01U, /*!< Transmit Command code to Flash, using SDR mode. */
    FLEXSPI_COMMAND_RADDR_SDR = 0x02U, /*!< Transmit Row Address to Flash, using SDR mode. */
    FLEXSPI_COMMAND_CADDR_SDR = 0x03U, /*!< Transmit Column Address to Flash, using SDR mode. */
    FLEXSPI_COMMAND_MODE1_SDR = 0x04U, /*!< Transmit 1-bit Mode bits to Flash, using SDR mode. */
    FLEXSPI_COMMAND_MODE2_SDR = 0x05U, /*!< Transmit 2-bit Mode bits to Flash, using SDR mode. */
    FLEXSPI_COMMAND_MODE4_SDR = 0x06U, /*!< Transmit 4-bit Mode bits to Flash, using SDR mode. */
    FLEXSPI_COMMAND_MODE8_SDR = 0x07U, /*!< Transmit 8-bit Mode bits to Flash, using SDR mode. */
    FLEXSPI_COMMAND_WRITE_SDR = 0x08U, /*!< Transmit Programming Data to Flash, using SDR mode. */
    FLEXSPI_COMMAND_READ_SDR = 0x09U, /*!< Receive Read Data from Flash, using SDR mode. */
    FLEXSPI_COMMAND_LEARN_SDR = 0x0AU, /*!< Receive Read Data or Preamble bit from Flash, SDR mode.
                                        */
    FLEXSPI_COMMAND_DATSZ_SDR = 0x0BU, /*!< Transmit Read/Program Data size (byte) to Flash, SDR
                                           mode. */
    FLEXSPI_COMMAND_DUMMY_SDR = 0x0CU, /*!< Leave data lines undriven by FlexSPI controller.*/
    FLEXSPI_COMMAND_DUMMY_RWDS_SDR = 0x0DU, /*!< Leave data lines undriven by FlexSPI controller,
                                             *   dummy cycles decided by RWDS. */
    FLEXSPI_COMMAND_DDR = 0x21U, /*!< Transmit Command code to Flash, using DDR mode. */
    FLEXSPI_COMMAND_RADDR_DDR = 0x22U, /*!< Transmit Row Address to Flash, using DDR mode. */
    FLEXSPI_COMMAND_CADDR_DDR = 0x23U, /*!< Transmit Column Address to Flash, using DDR mode. */
    FLEXSPI_COMMAND_MODE1_DDR = 0x24U, /*!< Transmit 1-bit Mode bits to Flash, using DDR mode. */
    FLEXSPI_COMMAND_MODE2_DDR = 0x25U, /*!< Transmit 2-bit Mode bits to Flash, using DDR mode. */
    FLEXSPI_COMMAND_MODE4_DDR = 0x26U, /*!< Transmit 4-bit Mode bits to Flash, using DDR mode. */
    FLEXSPI_COMMAND_MODE8_DDR = 0x27U, /*!< Transmit 8-bit Mode bits to Flash, using DDR mode. */
    FLEXSPI_COMMAND_WRITE_DDR = 0x28U, /*!< Transmit Programming Data to Flash, using DDR mode. */
    FLEXSPI_COMMAND_READ_DDR = 0x29U, /*!< Receive Read Data from Flash, using DDR mode. */
    FLEXSPI_COMMAND_LEARN_DDR = 0x2AU, /*!< Receive Read Data or Preamble bit from Flash, DDR mode.
                                        */
    FLEXSPI_COMMAND_DATSZ_DDR = 0x2BU, /*!< Transmit Read/Program Data size (byte) to Flash, DDR
                                           mode. */
    FLEXSPI_COMMAND_DUMMY_DDR = 0x2CU, /*!< Leave data lines undriven by FlexSPI controller.*/
    FLEXSPI_COMMAND_DUMMY_RWDS_DDR = 0x2DU, /*!< Leave data lines undriven by FlexSPI controller,
                                             *   dummy cycles decided by RWDS. */
    FLEXSPI_COMMAND_JUMP_ON_CS = 0x1FU, /*!< Stop execution, deassert CS and save operand[7:0] as
                                         * the instruction start pointer for next sequence */
};

struct flexspi_nor_config {
    const char *name;
    const uint8_t *uuid;
    enum flexspi_port port;
    size_t capacity;
    size_t block_size;
    size_t page_size;
    uint8_t mfg_id;
    uint8_t mfg_device_type;
    uint8_t mfg_capacity;
    uint32_t time_block_erase_ms;
    uint32_t time_page_program_ms;
    uint32_t cr1;
    uint32_t cr2;
    uint8_t lut_id_read_status;
    uint8_t lut_id_read_id;
    uint8_t lut_id_erase;
    uint8_t lut_id_read;
    uint8_t lut_id_write;
    uint8_t lut_id_wr_enable;
    uint8_t lut_id_wr_disable;
};

struct flexspi_core_config {
    uintptr_t base;
    uint32_t cr4;
    uint32_t dllacr;
    uint32_t dllbcr;
    size_t lut_elements;
    const uint32_t *lut;
    size_t mem_length;
    const struct flexspi_nor_config *mem[];
};

int imx_flexspi_init(const struct flexspi_core_config *config);

#endif // INCLUDE_DRIVERS_MEMC_IMX_FLEXSPI_H
