#include <pb.h>
#include <plat.h>
#include "pl061.h"
#include <tinyprintf.h>
#include "gcov.h"

void plat_reset(void)
{
    gcov_final();
    asm ("mov r0, #0x18     \n" \
         "movw r1,#0x0026   \n" \
         "movt r1,#0x0002   \n" \
         "svc 0x00123456    \n");
}

