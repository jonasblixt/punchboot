/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Marten Svanfeldt <marten.svanfeldt@actia.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_MEMC_IMX_FLEXSPI_H
#define INCLUDE_DRIVERS_MEMC_IMX_FLEXSPI_H

#include <stdint.h>

enum flexspi_port {
    kFLEXSPI_PortA1 = 0x0U, /*!< Access flash on A1 port. */
    kFLEXSPI_PortA2,        /*!< Access flash on A2 port. */
    kFLEXSPI_PortB1,        /*!< Access flash on B1 port. */
    kFLEXSPI_PortB2,        /*!< Access flash on B2 port. */
    kFLEXSPI_PortCount
};


enum flexspi_command_type {
    kFLEXSPI_Command, /*!< FlexSPI operation: Only command, both TX and Rx buffer are ignored. */
    kFLEXSPI_Config,  /*!< FlexSPI operation: Configure device mode, the TX fifo size is fixed in LUT. */
    kFLEXSPI_Read,    /* /!< FlexSPI operation: Read, only Rx Buffer is effective. */
    kFLEXSPI_Write,   /* /!< FlexSPI operation: Read, only Tx Buffer is effective. */
};


struct flexspi_transfer {
    uint32_t deviceAddress;                 /*!< Operation device address. */
    enum flexspi_port port;               /*!< Operation port. */
    enum flexspi_command_type cmdType;    /*!< Execution command type. */
    uint8_t seqIndex;                       /*!< Sequence ID for command. */
    uint8_t SeqNumber;                      /*!< Sequence number for command. */
    uint8_t *data;                          /*!< Data buffer. */
    size_t dataSize;                        /*!< Data size in bytes. */
};


int imx_flexspi_init(uintptr_t base_);
int imx_flexspi_configure_flash(uint32_t port, uint32_t flash_size);
int imx_flexspi_transferblocking(struct flexspi_transfer *xfer);

#endif  // INCLUDE_DRIVERS_MEMC_IMX_FLEXSPI_H
