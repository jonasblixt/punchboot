#include <stdio.h>
#include <pb.h>
#include <plat.h>
#include "pl061.h"
#include "gcov.h"
#include <plat/test/semihosting.h>

void plat_reset(void)
{
    gcov_final();
    semihosting_sys_exit(0);
}

