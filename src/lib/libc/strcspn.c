#include <string.h>

size_t strcspn(const char *s, const char *reject)
{
    size_t i;
    for (i = 0; s[i] && strchr(reject, s[i]) == NULL; i++)
        ;
    return i;
}
