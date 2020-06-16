/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Actia Nordic AB
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_GPIO_H_
#define PLAT_IMX_GPIO_H_

#include <pb/pb.h>

void gpio_set_pin(uint8_t bank, uint8_t pin, uint8_t value);
uint8_t gpio_get_pin(uint8_t bank, uint8_t pin);

#endif //PLAT_IMX_GPIO_H
