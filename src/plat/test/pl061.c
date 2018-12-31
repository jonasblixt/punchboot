#include <pb.h>
#include <io.h>
#include <tinyprintf.h>

#include "pl061.h"

static __iomem _base;

void pl061_init(__iomem base)
{
    uint32_t id = 0;
    _base = base;

    id = pb_read32(_base + 0xFE0);

    UNUSED(id);
}

void pl061_configure_direction(uint8_t pin, uint8_t dir)
{
    uint32_t dir_reg = pb_read32(_base + 0x400);

    if (dir)
        dir_reg |= (1 << pin);
    else
        dir_reg &= ~(1 << pin);

    pb_write32(dir_reg, _base + 0x400);
}

void pl061_set_value(uint8_t pin, uint8_t value)
{
    uint32_t val_reg = pb_read32(_base + (1 << (pin + 2)));

    if (value)
        val_reg |= (1 << pin);
    else
        val_reg &= ~(1 << pin);

    pb_write32(val_reg, _base + (1 << (pin + 2)));
}
