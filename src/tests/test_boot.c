#include <plat/test/semihosting.h>

#include "test.h"

void test_main(void) 
{
    LOG_INFO("Boot test");
    long fd = semihosting_file_open("arne.txt", 6);
    const char *test = "Hello\n";
    unsigned int sz = 6;
    semihosting_file_write(fd, &sz, (uintptr_t) test);
    semihosting_file_close(fd);
    LOG_INFO("Boot test end");
}
