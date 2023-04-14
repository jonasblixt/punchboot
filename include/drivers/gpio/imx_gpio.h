/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef DRIVERS_GPIO_IMX_GPIO_H
#define DRIVERS_GPIO_IMX_GPIO_H

#include <stdint.h>

int imx_gpio_init(uintptr_t base);
void imx_gpio_set_pin(uint8_t bank, uint8_t pin, uint8_t value);
uint8_t imx_gpio_get_pin(uint8_t bank, uint8_t pin);

#endif // DRIVERS_GPIO_IMX_GPIO_H
