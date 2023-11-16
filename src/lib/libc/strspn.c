#include <string.h>

size_t strspn(const char *s, const char *accept)
{
  size_t i;
  for (i = 0; s[i] && strchr(accept, s[i]) != NULL; i++);
  return i;
}
