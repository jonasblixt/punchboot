#include <board.h>
#include <plat.h>
#include <plat/test/semihosting.h>
#include <tinyprintf.h>
#include <assert.h>

#include "test.h"
#include "gcov.h"

void __assert_func(const char *fn,
                   int line_no,
                   const char *assert_func,
                   const char *asdf)
{
    tfp_printf("Assert failed %s:%i (%s)\n\r",fn, line_no, assert_func);
    semihosting_sys_exit(1);
}

void pb_main(void) 
{

    board_init();
	gcov_init();
    test_main();
	gcov_final();
    semihosting_sys_exit(0);
}
