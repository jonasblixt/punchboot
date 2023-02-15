#ifndef INCLUDE_ASM_H_
#define INCLUDE_ASM_H_

#define func(__func) .global __func; .type __func, STT_FUNC; __func:

#endif  // INCLUDE_ASM_H_
