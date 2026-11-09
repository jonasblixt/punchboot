/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <drivers/pinctrl/imx_iomuxc.h>
#include <pb/mmio.h>

void imx_iomuxc_setup(const struct imx_pin *pins, size_t length)
{
    for (size_t n = 0; n < length; n++) {
        const struct imx_pin *pin = &pins[n];

        mmio_write_32(pin->mux_reg, pin->mux);

        if (pin->pad != IMX_PAD_KEEP)
            mmio_write_32(pin->pad_reg, pin->pad);

        if (pin->input_reg != 0)
            mmio_write_32(pin->input_reg, pin->input);
    }
}
