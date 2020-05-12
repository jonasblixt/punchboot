#ifndef INCLUDE_PB_FLETCHER_H_
#define INCLUDE_PB_FLETCHER_H_

#include <stdint.h>
#include <stddef.h>

uint16_t fletcher8(const uint8_t *data, size_t bytes);

#endif  // INCLUDE_PB_FLETCHER_H_
