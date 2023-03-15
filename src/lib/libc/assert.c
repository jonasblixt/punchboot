#include <stdio.h>
#include <pb/pb.h>
#include <pb/assert.h>

void __assert(const char *file, unsigned int line)
{
    printf("ASSERT: %s:%d\n\r", file, line);
    while (1);
}
