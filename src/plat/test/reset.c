#include <pb.h>
#include <plat.h>
#include "pl061.h"
#include <tinyprintf.h>
#include "gcov.h"
#include <plat/test/semihosting.h>

void plat_reset(void)
{
    gcov_final();
    semihosting_sys_exit(0);
/*    asm ("mov r0, #0x18     \n" \
         "movw r1,#0x0026   \n" \
         "movt r1,#0x0002   \n" \
         "svc 0x00123456    \n");
*/
}

