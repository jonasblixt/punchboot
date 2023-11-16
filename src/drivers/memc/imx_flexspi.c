/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Marten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/pb.h>
#include <arch/arch.h>
#include <pb/delay.h>
#include <pb/mmio.h>
#include <pb/errors.h>

#include <drivers/memc/imx_flexspi.h>

#include "imx_flexspi_private.h"

static uintptr_t flexspi_base;

static inline bool imx_flexspi_getbusidle(void)
{
    static const uint32_t mask = FLEXSPI_STS0_SEQIDLE | FLEXSPI_STS0_ARBIDLE;

    uint32_t val = mmio_read_32(flexspi_base + FLEXSPI_STS0);

    return (val & mask) == mask;
}


int imx_flexspi_init(uintptr_t base_)
{
    flexspi_base = base_;

    /* Reset */
    mmio_clrbits_32(flexspi_base + FLEXSPI_MCR0, FLEXSPI_MCR0_MDIS);
    mmio_setbits_32(flexspi_base + FLEXSPI_MCR0, FLEXSPI_MCR0_SWRESET);

    /* Wait for reset */
    while((mmio_read_32(flexspi_base + FLEXSPI_MCR0) & FLEXSPI_MCR0_SWRESET))
        ;

    /* Setup configuration. Everything hard-coded for now, should be configurable */
    mmio_write_32(flexspi_base + FLEXSPI_MCR0,
        (0x00 << 4) |  /* RXCLKSRC internal */
        (1 << 12)  |   /* Doze enable*/
        (0xFF << 16) | /* IP grant timeout */
        (0xFF << 24)   /* AHB grant timeout */
    );

    mmio_write_32(flexspi_base + FLEXSPI_MCR1,
        (0xFFFF << 16) | /* SEQ execution timeout */
        (0xFFFF << 0)    /* AHB access timeout */
    );

    mmio_clrsetbits_32(flexspi_base + FLEXSPI_MCR2,
        FLEXSPI_MCR2_RESUMEWAIT_MASK | FLEXSPI_MCR2_SCKBDIFFOPT |
        FLEXSPI_MCR2_SAMEDEVICEEN | FLEXSPI_MCR2_CLRAHBBUFOPT,

        FLEXSPI_MCR2_RESUMEWAIT(20)
    );

    /* AHB control */
    mmio_clrsetbits_32(flexspi_base + FLEXSPI_AHBCR,
        FLEXSPI_AHBCR_CACHABLEEN | FLEXSPI_AHBCR_BUFFERABLEEN |
        FLEXSPI_AHBCR_PREFETCHEN | FLEXSPI_AHBCR_READADDROPT,

        FLEXSPI_AHBCR_CACHABLEEN | FLEXSPI_AHBCR_BUFFERABLEEN |
        FLEXSPI_AHBCR_PREFETCHEN | FLEXSPI_AHBCR_READADDROPT
    );

    /* AHB buffers */
    for (int i = 0; i < 4; i++) {
        mmio_write_32(flexspi_base + FLEXSPI_AHBRXBUFCR0(i),
            FLEXSPI_AHBRXBUFCR0_BUFSZ(256)
        );
    }

    /* TX&RX fifos */
    mmio_clrsetbits_32(flexspi_base + FLEXSPI_IPRXFCR,
        FLEXSPI_IPRXFCR_RXWMRK_MASK,
        FLEXSPI_IPRXFCR_RXWMRK(8)
    );

    mmio_clrsetbits_32(flexspi_base + FLEXSPI_IPTXFCR,
        FLEXSPI_IPTXFCR_TXWMRK_MASK,
        FLEXSPI_IPTXFCR_TXWMRK(8)
    );

    /* Reset size of all attached flashes */
    for (int i = 0; i < 4; i++) {
        mmio_write_32(flexspi_base + FLEXSPI_FLSHCR0(i), 0);
    }

    return PB_OK;
}

enum _flexspi_command {
        kFLEXSPI_Command_STOP = 0x00U,           /*!< Stop execution, deassert CS. */
        kFLEXSPI_Command_SDR = 0x01U,            /*!< Transmit Command code to Flash, using SDR mode. */
        kFLEXSPI_Command_RADDR_SDR = 0x02U,      /*!< Transmit Row Address to Flash, using SDR mode. */
        kFLEXSPI_Command_CADDR_SDR = 0x03U,      /*!< Transmit Column Address to Flash, using SDR mode. */
        kFLEXSPI_Command_MODE1_SDR = 0x04U,      /*!< Transmit 1-bit Mode bits to Flash, using SDR mode. */
        kFLEXSPI_Command_MODE2_SDR = 0x05U,      /*!< Transmit 2-bit Mode bits to Flash, using SDR mode. */
        kFLEXSPI_Command_MODE4_SDR = 0x06U,      /*!< Transmit 4-bit Mode bits to Flash, using SDR mode. */
        kFLEXSPI_Command_MODE8_SDR = 0x07U,      /*!< Transmit 8-bit Mode bits to Flash, using SDR mode. */
        kFLEXSPI_Command_WRITE_SDR = 0x08U,      /*!< Transmit Programming Data to Flash, using SDR mode. */
        kFLEXSPI_Command_READ_SDR = 0x09U,       /*!< Receive Read Data from Flash, using SDR mode. */
        kFLEXSPI_Command_LEARN_SDR = 0x0AU,      /*!< Receive Read Data or Preamble bit from Flash, SDR mode. */
        kFLEXSPI_Command_DATSZ_SDR = 0x0BU,      /*!< Transmit Read/Program Data size (byte) to Flash, SDR mode. */
        kFLEXSPI_Command_DUMMY_SDR = 0x0CU,      /*!< Leave data lines undriven by FlexSPI controller.*/
        kFLEXSPI_Command_DUMMY_RWDS_SDR = 0x0DU, /*!< Leave data lines undriven by FlexSPI controller,
                                                  *   dummy cycles decided by RWDS. */
        kFLEXSPI_Command_DDR = 0x21U,            /*!< Transmit Command code to Flash, using DDR mode. */
        kFLEXSPI_Command_RADDR_DDR = 0x22U,      /*!< Transmit Row Address to Flash, using DDR mode. */
        kFLEXSPI_Command_CADDR_DDR = 0x23U,      /*!< Transmit Column Address to Flash, using DDR mode. */
        kFLEXSPI_Command_MODE1_DDR = 0x24U,      /*!< Transmit 1-bit Mode bits to Flash, using DDR mode. */
        kFLEXSPI_Command_MODE2_DDR = 0x25U,      /*!< Transmit 2-bit Mode bits to Flash, using DDR mode. */
        kFLEXSPI_Command_MODE4_DDR = 0x26U,      /*!< Transmit 4-bit Mode bits to Flash, using DDR mode. */
        kFLEXSPI_Command_MODE8_DDR = 0x27U,      /*!< Transmit 8-bit Mode bits to Flash, using DDR mode. */
        kFLEXSPI_Command_WRITE_DDR = 0x28U,      /*!< Transmit Programming Data to Flash, using DDR mode. */
        kFLEXSPI_Command_READ_DDR = 0x29U,       /*!< Receive Read Data from Flash, using DDR mode. */
        kFLEXSPI_Command_LEARN_DDR = 0x2AU,      /*!< Receive Read Data or Preamble bit from Flash, DDR mode. */
        kFLEXSPI_Command_DATSZ_DDR = 0x2BU,      /*!< Transmit Read/Program Data size (byte) to Flash, DDR mode. */
        kFLEXSPI_Command_DUMMY_DDR = 0x2CU,      /*!< Leave data lines undriven by FlexSPI controller.*/
        kFLEXSPI_Command_DUMMY_RWDS_DDR = 0x2DU, /*!< Leave data lines undriven by FlexSPI controller,
                                                  *   dummy cycles decided by RWDS. */
        kFLEXSPI_Command_JUMP_ON_CS = 0x1FU,     /*!< Stop execution, deassert CS and save operand[7:0] as the
                                                  *   instruction start pointer for next sequence */
};

#define CUSTOM_LUT_LENGTH 64


#define NOR_CMD_LUT_SEQ_IDX_READ_FAST_QUAD     0
#define NOR_CMD_LUT_SEQ_IDX_READSTATUS         1
#define NOR_CMD_LUT_SEQ_IDX_WRITEENABLE        2
#define NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE       3
#define NOR_CMD_LUT_SEQ_IDX_ERASESECTOR        4
#define NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD   5
#define NOR_CMD_LUT_SEQ_IDX_ERASECHIP          6
#define NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE 7
#define NOR_CMD_LUT_SEQ_IDX_READ_NORMAL        8
#define NOR_CMD_LUT_SEQ_IDX_READID             9
#define NOR_CMD_LUT_SEQ_IDX_WRITESTATUSREG     10
#define NOR_CMD_LUT_SEQ_IDX_ENTERQPI           11
#define NOR_CMD_LUT_SEQ_IDX_EXITQPI            12
#define NOR_CMD_LUT_SEQ_IDX_READSTATUSREG      13
#define NOR_CMD_LUT_SEQ_IDX_READ_FAST          14
#define NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK         15

#define FLEXSPI_1PAD                0
#define FLEXSPI_2PAD                1
#define FLEXSPI_4PAD                2
#define FLEXSPI_8PAD                3

#define FLEXSPI_LUT_OPERAND0_MASK   (0xffu)
#define FLEXSPI_LUT_OPERAND0_SHIFT  (0U)
#define FLEXSPI_LUT_OPERAND0(x)     (((uint32_t) \
                                     (((uint32_t)(x)) << FLEXSPI_LUT_OPERAND0_SHIFT)) & \
                                     FLEXSPI_LUT_OPERAND0_MASK)
#define FLEXSPI_LUT_NUM_PADS0_MASK  (0x300u)
#define FLEXSPI_LUT_NUM_PADS0_SHIFT (8u)
#define FLEXSPI_LUT_NUM_PADS0(x)    (((uint32_t) \
                                     (((uint32_t)(x)) << FLEXSPI_LUT_NUM_PADS0_SHIFT)) & \
                                     FLEXSPI_LUT_NUM_PADS0_MASK)
#define FLEXSPI_LUT_OPCODE0_MASK    (0xfc00u)
#define FLEXSPI_LUT_OPCODE0_SHIFT   (10u)
#define FLEXSPI_LUT_OPCODE0(x)      (((uint32_t) \
                                     (((uint32_t)(x)) << FLEXSPI_LUT_OPCODE0_SHIFT)) & \
                                     FLEXSPI_LUT_OPCODE0_MASK)
#define FLEXSPI_LUT_OPERAND1_MASK   (0xff0000u)
#define FLEXSPI_LUT_OPERAND1_SHIFT  (16U)
#define FLEXSPI_LUT_OPERAND1(x)     (((uint32_t) \
                                     (((uint32_t)(x)) << FLEXSPI_LUT_OPERAND1_SHIFT)) & \
                                     FLEXSPI_LUT_OPERAND1_MASK)
#define FLEXSPI_LUT_NUM_PADS1_MASK  (0x3000000u)
#define FLEXSPI_LUT_NUM_PADS1_SHIFT (24u)
#define FLEXSPI_LUT_NUM_PADS1(x)    (((uint32_t) \
                                     (((uint32_t)(x)) << FLEXSPI_LUT_NUM_PADS1_SHIFT)) & \
                                     FLEXSPI_LUT_NUM_PADS1_MASK)
#define FLEXSPI_LUT_OPCODE1_MASK    (0xfc000000u)
#define FLEXSPI_LUT_OPCODE1_SHIFT   (26u)
#define FLEXSPI_LUT_OPCODE1(x)      (((uint32_t)(((uint32_t)(x)) << FLEXSPI_LUT_OPCODE1_SHIFT)) & \
                                    FLEXSPI_LUT_OPCODE1_MASK)

#define FLEXSPI_LUT_SEQ(cmd0, pad0, op0, cmd1, pad1, op1)  \
    (FLEXSPI_LUT_OPERAND0(op0) | FLEXSPI_LUT_NUM_PADS0(pad0) | \
     FLEXSPI_LUT_OPCODE0(cmd0) | FLEXSPI_LUT_OPERAND1(op1) | \
     FLEXSPI_LUT_NUM_PADS1(pad1) | FLEXSPI_LUT_OPCODE1(cmd1))

const uint32_t customNorLUT[CUSTOM_LUT_LENGTH] = {
    /* Read ID */
    [4 * NOR_CMD_LUT_SEQ_IDX_READID] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                       FLEXSPI_1PAD,
                                                       0x9F,
                                                       kFLEXSPI_Command_READ_SDR,
                                                       FLEXSPI_1PAD,
                                                       0x04),

    /* Read status */
    [4 * NOR_CMD_LUT_SEQ_IDX_READSTATUS] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                           FLEXSPI_1PAD,
                                                           0x05,
                                                           kFLEXSPI_Command_READ_SDR,
                                                           FLEXSPI_1PAD,
                                                           0x04),

    /* Fast read */
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                          FLEXSPI_1PAD,
                                                          0x0B,
                                                          kFLEXSPI_Command_RADDR_SDR,
                                                          FLEXSPI_1PAD,
                                                          0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_READ_FAST + 1] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_DUMMY_SDR,
                                                              FLEXSPI_1PAD,
                                                              0x08,
                                                              kFLEXSPI_Command_READ_SDR,
                                                              FLEXSPI_1PAD,
                                                              0x04),

    /* Write enable */
    [4 * NOR_CMD_LUT_SEQ_IDX_WRITEENABLE] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                            FLEXSPI_1PAD,
                                                            0x06,
                                                            kFLEXSPI_Command_STOP,
                                                            FLEXSPI_1PAD,
                                                            0x00),

    /* Write disable */
    [4 * NOR_CMD_LUT_SEQ_IDX_WRITEDISABLE] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                             FLEXSPI_1PAD,
                                                             0x04,
                                                             kFLEXSPI_Command_STOP,
                                                             FLEXSPI_1PAD,
                                                             0x00),

    /* Erase 4kb sector */
    [4 * NOR_CMD_LUT_SEQ_IDX_ERASESECTOR] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                            FLEXSPI_1PAD,
                                                            0xD7,
                                                            kFLEXSPI_Command_RADDR_SDR,
                                                            FLEXSPI_1PAD,
                                                            0x18),

    /* Erase 64kb block */
    [4U * NOR_CMD_LUT_SEQ_IDX_ERASEBLOCK] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                            FLEXSPI_1PAD,
                                                            0xD8,
                                                            kFLEXSPI_Command_RADDR_SDR,
                                                            FLEXSPI_1PAD,
                                                            0x18),

    /* Page Program - single mode */
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                                   FLEXSPI_1PAD,
                                                                   0x02,
                                                                   kFLEXSPI_Command_RADDR_SDR,
                                                                   FLEXSPI_1PAD,
                                                                   0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_SINGLE + 1] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_WRITE_SDR,
                                                                       FLEXSPI_1PAD,
                                                                       0x04,
                                                                       kFLEXSPI_Command_STOP,
                                                                       FLEXSPI_1PAD,
                                                                       0x00),

    /* Page Program - quad mode */
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_SDR,
                                                                 FLEXSPI_1PAD,
                                                                 0x32,
                                                                 kFLEXSPI_Command_RADDR_SDR,
                                                                 FLEXSPI_1PAD,
                                                                 0x18),
    [4 * NOR_CMD_LUT_SEQ_IDX_PAGEPROGRAM_QUAD + 1] = FLEXSPI_LUT_SEQ(kFLEXSPI_Command_WRITE_SDR,
                                                                     FLEXSPI_4PAD,
                                                                     0x04,
                                                                     kFLEXSPI_Command_STOP,
                                                                     FLEXSPI_1PAD,
                                                                     0x00),
};

#define FLEXSPI_LUT_KEY_VAL (0x5AF05AF0ul)

static void imx_flexspi_updatelut(uint32_t index,
                             const uint32_t *cmd, uint32_t count)
{
    uint8_t i = 0;

    /* Wait for bus idle before change flash configuration. */
    while (!imx_flexspi_getbusidle())
        ;

    /* Unlock LUT for update. */
    mmio_write_32(flexspi_base + 0x18, FLEXSPI_LUT_KEY_VAL);
    mmio_write_32(flexspi_base + 0x1C, 0x02);

    for (i = index; i < count; i++) {
        mmio_write_32(flexspi_base + 0x200 + (4 * i), cmd[i]);
    }

    /* Lock LUT. */
    mmio_write_32(flexspi_base + 0x18, FLEXSPI_LUT_KEY_VAL);
    mmio_write_32(flexspi_base + 0x1C, 0x01);
}

enum _flexspi_read_sample_clock {
        kFLEXSPI_ReadSampleClkLoopbackInternally = 0x0U,
        kFLEXSPI_ReadSampleClkLoopbackFromDqsPad = 0x1U,
        kFLEXSPI_ReadSampleClkLoopbackFromSckPad = 0x2U,
        kFLEXSPI_ReadSampleClkExternalInputFromDqsPad = 0x3U,
};

static uint32_t imx_flexspi_configuredll(void)
{
    bool isUnifiedConfig = true;
    uint32_t flexspiDllValue;
    uint32_t dllValue;
    uint32_t temp;


    uint8_t rxSampleClock = (mmio_read_32(flexspi_base) & 0x30u) >> 4;
    switch (rxSampleClock) {
        case kFLEXSPI_ReadSampleClkLoopbackInternally:
        case kFLEXSPI_ReadSampleClkLoopbackFromDqsPad:
        case kFLEXSPI_ReadSampleClkLoopbackFromSckPad:
            isUnifiedConfig = true;
            break;
        case kFLEXSPI_ReadSampleClkExternalInputFromDqsPad:
            isUnifiedConfig = false;
            break;
        default:
            break;
    }

    if (isUnifiedConfig) {
        flexspiDllValue = 0x100ul;
    } else {
        if (120000000 >= 100 * 1000000UL) {
            /* DLLEN = 1, SLVDLYTARGET = 0xF, */
            flexspiDllValue = 0x1 << 0 | 0x0F << 0x3;
        } else {
            temp = 0 * 1000; /* Convert data valid time in ns to ps. */
            dllValue = temp / 75;
            if (dllValue * 75 < temp) {
                dllValue++;
            }
            flexspiDllValue = 1 << 8 | dllValue << 9;
        }
    }
    return flexspiDllValue;
}

static void imx_flexspi_setflashconfig(void)
{
    uint32_t configValue = 0;

    /* Wait for bus idle before change flash configuration. */
    /* Configure the flash size */
    while (!imx_flexspi_getbusidle())
        ;

    // TODO: Add config struct for the following parameters

    /* Configure flash parameters. */
    mmio_write_32(flexspi_base + 0x70, 0x2 << 16 | 0 << 15 | 0x3 << 5 | 0x3 << 0 | 0 << 11 | 0 << 10);

    /* Configure AHB operation items. */
    configValue = mmio_read_32(flexspi_base + 0x80);

    configValue &= ~(0x70000000U | 0xFFF0000U | 0xE000U | 0xF00U | 0xE0U);

    configValue |= (0 << 28 | 0 << 16);

    configValue |= (14 << 0 | 1 << 5);

    mmio_write_32(flexspi_base + 0x80, configValue);

    /* Configure DLL. */
    mmio_write_32(flexspi_base + 0xC0, imx_flexspi_configuredll());

    configValue = mmio_read_32(flexspi_base + 0x94);

    /* Configure write mask. */
    configValue |= 0x1;
    configValue &= ~0x4;
    configValue |= 0 << 3;

    mmio_write_32(flexspi_base + 0x94, configValue);

    /* Exit stop mode. */
    configValue = mmio_read_32(flexspi_base);
    configValue &= ~0x2;
    mmio_write_32(flexspi_base, configValue);
}

int imx_flexspi_configure_flash(uint32_t port, uint32_t flash_size)
{
    /* Configure the flash size */
    while (!imx_flexspi_getbusidle())
        ;

    mmio_write_32(flexspi_base + FLEXSPI_FLSHCR0(port), flash_size);

    imx_flexspi_setflashconfig();

    /* Flash parameters, hard coded */

    imx_flexspi_updatelut(0, customNorLUT, CUSTOM_LUT_LENGTH);

    return PB_OK;
}

static int imx_flexspi_checkandclearerror(uint32_t status)
{
    int rc = 0;

    /* Check for error. */
    status &= FLEXSPI_INT_SEQTIMEOUTEN |
              FLEXSPI_INT_IPCMDERREN |
              FLEXSPI_INT_IPCMDGEEN;

    if (status) {
        /* Select the correct error code.. */
        if (status & FLEXSPI_INT_SEQTIMEOUTEN) {
            rc = -PB_ERR_IO;
        } else if (status & FLEXSPI_INT_IPCMDERREN) {
            rc = -PB_ERR_IO;
        } else if (status & FLEXSPI_INT_IPCMDGEEN) {
            rc = -PB_ERR_IO;
        } else {
            LOG_ERR("Unhandled IRQ (%x)", status);
            rc = -PB_ERR_IO;
        }

        /* Clear the flags. */
        mmio_write_32(flexspi_base + FLEXSPI_INTR, status);

        /* Reset fifos. These flags clear automatically. */
        mmio_clrsetbits_32(flexspi_base + FLEXSPI_IPTXFCR, 0, FLEXSPI_IPTXFCR_CLRIPTXF_MASK);
        mmio_clrsetbits_32(flexspi_base + FLEXSPI_IPRXFCR, 0, FLEXSPI_IPRXFCR_CLRIPRXF_MASK);
    }

    return rc;
}


static int imx_flexspi_writeblocking(uint8_t *buffer, size_t size)
{
    uint8_t txWatermark;
    uint32_t status;
    int rc = 0;
    uint32_t i = 0;

    txWatermark = ((mmio_read_32(flexspi_base + FLEXSPI_IPTXFCR) & FLEXSPI_IPTXFCR_TXWMRK_MASK)
                    >> FLEXSPI_IPTXFCR_TXWMRK_SHIFT) + 1;

    /* Send data buffer */
    while (size) {
        /* Wait until there is room in the fifo. This also checks for errors. */
        do {
            status = mmio_read_32(flexspi_base + FLEXSPI_INTR);
        } while (!(status & FLEXSPI_INT_IPTXWEEN));

        rc = imx_flexspi_checkandclearerror(status);

        if (rc != 0) {
            LOG_ERR("xfer error (%x)", status);
            return rc;
        }

        /* Write watermark level data into tx fifo . */
        if (size >= 8 * txWatermark) {
            for (i = 0; i < 2 * txWatermark; i++) {
                mmio_write_32(flexspi_base + FLEXSPI_TFDR0 + i*4, *((uint32_t *) buffer));
                buffer += 4;
            }

            size = size - 8 * txWatermark;
        } else {
            for (i = 0; i < (size / 4 + 1); i++) {
                mmio_write_32(flexspi_base + FLEXSPI_TFDR0 + i*4, *((uint32_t *) buffer));
                buffer += 4;
            }
            size = 0;
        }

        /* Push a watermark level datas into IP TX FIFO. */
        mmio_clrsetbits_32(flexspi_base + FLEXSPI_INTR, 0, FLEXSPI_INT_IPTXWEEN);
    }

    return rc;
}

static int imx_flexspi_readblocking(uint8_t *buffer, size_t size)
{
    uint8_t rxWatermark;
    uint32_t status;
    int rc = 0;
    uint32_t i = 0;

    rxWatermark = ((mmio_read_32(flexspi_base + FLEXSPI_IPRXFCR) & FLEXSPI_IPRXFCR_RXWMRK_MASK)
                    >> FLEXSPI_IPRXFCR_RXWMRK_SHIFT) + 1;

    /* Send data buffer */
    while (size) {
        if (size >= 8 * rxWatermark) {
            /* Wait until there is room in the fifo. This also checks for errors. */
            while (!((status = mmio_read_32(flexspi_base + FLEXSPI_INTR))
                    & FLEXSPI_INT_IPRXWAEN)) {
                rc = imx_flexspi_checkandclearerror(status);

                if (rc != 0)
                    return rc;
            }
        } else {
            /* Wait fill level. This also checks for errors. */
            while (size > ((((mmio_read_32(flexspi_base + FLEXSPI_IPRXFSTS)) & FLEXSPI_IPRXFSTS_FILL_MASK)
                            >> FLEXSPI_IPRXFSTS_FILL_SHIFT) * 8U)) {
                rc = imx_flexspi_checkandclearerror(mmio_read_32(flexspi_base + FLEXSPI_INTR));

                if (rc != 0)
                    return rc;
            }
        }

        rc = imx_flexspi_checkandclearerror(mmio_read_32(flexspi_base + FLEXSPI_INTR));

        if (rc != 0)
            return rc;

        /* Read watermark level data from rx fifo . */
        if (size >= 8 * rxWatermark) {
            for (i = 0; i < 2 * rxWatermark; i++) {
                *((uint32_t *) buffer) = mmio_read_32(flexspi_base + FLEXSPI_RFDR0 + i * 4);
                buffer += 4;
            }

            size = size - 8 * rxWatermark;
        } else {
            // size has to be dividable by 4 (is checked in transferblocking)
            for (i = 0; i < (size / 4); i++) {
                *((uint32_t *) buffer) = mmio_read_32(flexspi_base + FLEXSPI_RFDR0 + i * 4);
                buffer += 4;
            }
            size = 0;
        }

        /* Pop out a watermark level datas from IP RX FIFO. */
        mmio_clrsetbits_32(flexspi_base + FLEXSPI_INTR, 0, FLEXSPI_INT_IPRXWAEN);
    }

    return rc;
}


int imx_flexspi_transferblocking(struct flexspi_transfer *xfer)
{
    uintptr_t port_off = 0;
    uint32_t configValue = 0;
    int rc = 0;

    /* Clear sequence pointer before sending data to external devices. */
    switch (xfer->port) {
        case kFLEXSPI_PortA1:
            port_off = FLEXSPI_FLSHA1CR2;
        break;
        case kFLEXSPI_PortA2:
            port_off = FLEXSPI_FLSHA2CR2;
        break;
        case kFLEXSPI_PortB1:
            port_off = FLEXSPI_FLSHB1CR2;
        break;
        case kFLEXSPI_PortB2:
            port_off = FLEXSPI_FLSHB2CR2;
        break;
        default:
            return -PB_ERR_INVALID_ARGUMENT;
    }

    mmio_clrsetbits_32(flexspi_base + port_off, 0, FLEXSPI_FLSHCR2_CLRINSTRPTR_MASK);

    /* Clear former pending status before start this tranfer. */
    mmio_clrsetbits_32(flexspi_base + FLEXSPI_INTR, 0,
            FLEXSPI_INTR_AHBCMDERR_MASK | FLEXSPI_INTR_IPCMDERR_MASK
            | FLEXSPI_INTR_AHBCMDGE_MASK |
            FLEXSPI_INTR_IPCMDGE_MASK);

    /* Configure base addresss. */
    mmio_write_32(flexspi_base + FLEXSPI_IPCR0, xfer->deviceAddress);

    /* Reset fifos. */
    mmio_clrsetbits_32(flexspi_base + FLEXSPI_IPTXFCR, 0, FLEXSPI_IPTXFCR_CLRIPTXF_MASK);
    mmio_clrsetbits_32(flexspi_base + FLEXSPI_IPRXFCR, 0, FLEXSPI_IPRXFCR_CLRIPRXF_MASK);

    /* Configure data size. */
    if ((xfer->cmdType == kFLEXSPI_Read) || (xfer->cmdType == kFLEXSPI_Write)
            || (xfer->cmdType == kFLEXSPI_Config)) {
        configValue = FLEXSPI_IPCR1_IDATSZ(xfer->dataSize);
    }

    /* Configure sequence ID. */
    configValue |= FLEXSPI_IPCR1_ISEQID(xfer->seqIndex) | FLEXSPI_IPCR1_ISEQNUM(xfer->SeqNumber - 1);
    mmio_write_32(flexspi_base + FLEXSPI_IPCR1, configValue);

    /* Start Transfer. */
    mmio_clrsetbits_32(flexspi_base + FLEXSPI_IPCMD, 0, FLEXSPI_IPCMD_TRG_MASK);

    if ((xfer->cmdType == kFLEXSPI_Write) || (xfer->cmdType == kFLEXSPI_Config)) {
        rc = imx_flexspi_writeblocking(xfer->data, xfer->dataSize);
    } else if (xfer->cmdType == kFLEXSPI_Read) {
        if ((xfer->dataSize % 4) != 0) {
            LOG_ERR("Size must be divisible by 4");
            return -PB_ERR_INVALID_ARGUMENT;
        }
        rc = imx_flexspi_readblocking(xfer->data, xfer->dataSize);
    } else {
    }

    /* Wait for bus idle. */
    while (!imx_flexspi_getbusidle())
        ;

    if (xfer->cmdType == kFLEXSPI_Command) {
        rc = imx_flexspi_checkandclearerror(mmio_read_32(flexspi_base + FLEXSPI_INTR));
    }

    return rc;
}

