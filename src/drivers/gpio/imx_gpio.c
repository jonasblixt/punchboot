/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/mmio.h>
#include <drivers/gpio/imx_gpio.h>

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
#define GPIO_BANK_BASE(bank) (base + GPIO_REG_BANK_SIZE * bank)
#define GPIO_PIN(pin) (1 << (pin))

static uintptr_t base;

int imx_gpio_init(uintptr_t base_)
{
    base = base_;
    return 0;
}

void imx_gpio_set_pin(uint8_t bank, uint8_t pin, uint8_t value)
{
    /* Set pin value*/
    if (value)
        mmio_clrsetbits_32(GPIO_BANK_BASE(bank) + DR, 0, GPIO_PIN(pin));
    else
        mmio_clrsetbits_32(GPIO_BANK_BASE(bank) + DR, GPIO_PIN(pin), 0);

    /* Set direction output*/
    mmio_clrsetbits_32(GPIO_BANK_BASE(bank) + GDIR, 0, GPIO_PIN(pin));
}

uint8_t imx_gpio_get_pin(uint8_t bank, uint8_t pin)
{
    /* Set direction input*/
    mmio_clrsetbits_32(GPIO_BANK_BASE(bank) + GDIR, GPIO_PIN(pin), 0);

    /* Read bank value*/
    uint32_t bank_val = mmio_read_32(GPIO_BANK_BASE(bank) + PSR);

    return (bank_val >> pin) & 0x1;
}
