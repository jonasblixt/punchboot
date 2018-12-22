#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <plat/test/gcov.h>
#include <plat/test/semihosting.h>

static struct gcov_info *head;

#if (__GNUC__ >= 7)
#define GCOV_COUNTERS           9
#else
#error "Compiler not supported"
#endif

#define GCOV_TAG_FUNCTION_LENGTH    3

#define GCOV_DATA_MAGIC     ((unsigned int) 0x67636461)
#define GCOV_TAG_FUNCTION   ((unsigned int) 0x01000000)
#define GCOV_TAG_COUNTER_BASE   ((unsigned int) 0x01a10000)
#define GCOV_TAG_FOR_COUNTER(count)                 \
        (GCOV_TAG_COUNTER_BASE + ((unsigned int) (count) << 17))


void __gcov_init (struct gcov_info *p)
{
	p->next = head;
	head = p;
	tfp_printf("gcov_init: fn = %s\n", p->filename);

	long fd = semihosting_file_open(p->filename, 11);
	
	long f_length = semihosting_file_length(fd);
	tfp_printf ("  %li %libytes\n\r",fd, f_length);

	semihosting_file_close(fd);
}

void __gcov_merge_add (gcov_type *counters, unsigned n_counters) 
{
	UNUSED(counters);
	UNUSED(n_counters);

	/* Not Used */
}

void __gcov_flush (void) 
{
	/* Not used */
}

void __gcov_exit (void)
{
	/* Not used */
}

void gcov_init(void)
{
	/* Call gcov initalizers */
 	extern uint32_t __init_array_start, __init_array_end;
	void (**pctor)(void) = (void (**)(void)) &__init_array_start;
	void (**pctor_last)(void) = (void (**)(void)) &__init_array_end;
	
	for (; pctor < pctor_last; pctor++)
		(*pctor)();
  
}

uint32_t gcov_final(void)
{
	uint32_t gcov_version = 1;//GCOV_VERSION;
	size_t bytes_to_write = 0;

	struct gcov_info *info = head;

	for (;info; info = info->next) 
	{
		long fd = semihosting_file_open(info->filename, 6);
	
		tfp_printf ("fn: %s\n\r",info->filename);
	
		bytes_to_write = sizeof(uint32_t);
		semihosting_file_write(fd, &bytes_to_write, 
								(const uintptr_t) &gcov_version);	

		semihosting_file_close(fd);
	}

    return 0; /* TODO: What should the return value be*/
}


