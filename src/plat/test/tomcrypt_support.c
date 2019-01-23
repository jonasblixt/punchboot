#include <stdint.h>
#include <string.h>
#include <pb.h>
#include <stdlib.h>
#include <time.h>
#include <tinyprintf.h>
/* simplistic malloc functions needed by libtomcrypt */

#define HEAP_SZ 1024*1024
uint8_t heap[HEAP_SZ];
uint32_t heap_ptr = 0;

void *malloc(size_t size)
{
	if ((heap_ptr+size) > HEAP_SZ)
	{
		LOG_ERR("Out of memory");
		return NULL;
	}
	void * ptr = (void *) &heap[heap_ptr];
	heap_ptr += size;

	return ptr;
}

void free(void *ptr)
{
    UNUSED(ptr);
}

void *calloc(size_t nmemb, size_t size)
{
	return malloc (nmemb*size);
}

void *realloc(void *ptr, size_t size)
{
    void *new_ptr = malloc(size);
    memcpy(new_ptr,ptr,size);

	return new_ptr;
}

int rand(void)
{
	return 0;
}


const char *__locale_ctype_ptr(void)
{
    return NULL;
}

void qsort(void *base, size_t nmemb, size_t size,
			int(*compar)(const void*, const void*))
{

    UNUSED(base);
    UNUSED(nmemb);
    UNUSED(size);
    UNUSED(compar);

    LOG_ERR("qsort called");
    while(1);
}

clock_t clock(void)
{
    return 0;
}
