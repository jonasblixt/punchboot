/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Actia Nordic AB
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/io.h>
#include <plat/imx/gpio.h>

#define DR         (0x0)
#define GDIR       (0x4)
#define PSR        (0x8)
#define ICR1       (0xC)
#define ICR2       (0x10)
#define IMR        (0x14)
#define ISR        (0x18)
#define EDGE_SEL   (0x1C)
#define DR_SET     (0x84)
#define DR_CLEAR   (0x88)
#define DR_TOGGLE  (0x8C)

#define GPIO_REG_BANK_SIZE (0x10000)
#define GPIO_BANK_BASE(bank) (CONFIG_IMX_GPIO_BASE + GPIO_REG_BANK_SIZE * bank)
#define GPIO_PIN(pin) (1 << (pin))

void gpio_set_pin(uint8_t bank, uint8_t pin, uint8_t value) {
	/* Set pin value*/
	if (value)
		pb_setbit32(GPIO_PIN(pin), GPIO_BANK_BASE(bank) + DR);
	else
		pb_clrbit32(GPIO_PIN(pin), GPIO_BANK_BASE(bank) + DR);

	/* Set direction output*/
	pb_setbit32(GPIO_PIN(pin), GPIO_BANK_BASE(bank) + GDIR);
}

uint8_t gpio_get_pin(uint8_t bank, uint8_t pin){
	/* Set direction input*/
	pb_clrbit32(GPIO_PIN(pin), GPIO_BANK_BASE(bank) + GDIR);

	/* Read bank value*/
	uint32_t bank_val = pb_read32(GPIO_BANK_BASE(bank) + PSR);

	return (bank_val >> pin) & 0x1;
}
