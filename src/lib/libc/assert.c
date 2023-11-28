#include <pb/assert.h>
#include <pb/pb.h>
#include <stdio.h>

void __assert(const char *file, unsigned int line)
{
    printf("ASSERT: %s:%d\n\r", file, line);
    while (1)
        ;
}
