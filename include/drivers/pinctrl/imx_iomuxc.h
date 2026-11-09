/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The i.MX IOMUXC (i.MX6UL, i.MX8M, i.MX RT): a pin's function (mux), its
 * pad's electrical settings and, for a function that more than one pin can
 * have, which pin it uses (select input). A board lists the pins of a
 * peripheral in a table and sets them up with imx_iomuxc_setup().
 *
 * IMX6UL_PIN(), IMX8M_PIN() or IMXRT_PIN() makes the entry of a pin by its
 * name, the _IN() variants with a select input:
 *
 *   static const struct imx_pin uart_pins[] = {
 *       IMX6UL_PIN(UART2_TX_DATA, MUX_ALT(0), UART_PAD),
 *       IMX6UL_PIN(UART2_RX_DATA, MUX_ALT(0), UART_PAD),
 *   };
 */

#ifndef INCLUDE_DRIVERS_PINCTRL_IMX_IOMUXC_H
#define INCLUDE_DRIVERS_PINCTRL_IMX_IOMUXC_H

#include <stddef.h>
#include <stdint.h>

/*
 * An entry by the pin's name, with the platform's register names
 * (plat/imx6ul/pins.h, plat/imx8m/pins_imx8m*.h, plat/imxrt/pins.h). The _IN()
 * variants set the function's select input register to 'in'.
 */
#define IMX_PIN_IN(mux_reg, pad_reg, mux, pad, in_reg, in) \
    { mux_reg, pad_reg, mux, pad, in_reg, in }
#define IMX6UL_PIN_IN(n, mux, pad, in_reg, in) \
    IMX_PIN_IN(SW_MUX_CTL_PAD_##n, SW_PAD_CTL_PAD_##n, mux, pad, in_reg, in)
#define IMX6UL_PIN(n, mux, pad) IMX6UL_PIN_IN(n, mux, pad, 0, 0)
#define IMX8M_PIN_IN(n, mux, pad, in_reg, in) \
    IMX_PIN_IN(IOMUXC_MUX_##n, IOMUXC_PAD_##n, mux, pad, in_reg, in)
#define IMX8M_PIN(n, mux, pad) IMX8M_PIN_IN(n, mux, pad, 0, 0)
#define IMXRT_PIN_IN(n, mux, pad, in_reg, in) \
    IMX_PIN_IN(SW_MUX_CTL_PAD_##n, SW_PAD_CTL_PAD_##n, mux, pad, in_reg, in)
#define IMXRT_PIN(n, mux, pad) IMXRT_PIN_IN(n, mux, pad, 0, 0)

/** Leave the pad's settings as they are, the reset's or the ROM's */
#define IMX_PAD_KEEP           (0xffffffffu)

struct imx_pin {
    uint32_t mux_reg;
    uint32_t pad_reg;
    uint32_t mux; /*!< MUX_ALT(n), with SION */
    uint32_t pad; /*!< The pad's settings, or IMX_PAD_KEEP */
    uint32_t input_reg; /*!< The function's select input register, 0: none */
    uint32_t input; /*!< Its value: this pin */
};

/** Set up the pins, each mux, pad and select input in table order */
void imx_iomuxc_setup(const struct imx_pin *pins, size_t length);

#endif // INCLUDE_DRIVERS_PINCTRL_IMX_IOMUXC_H
