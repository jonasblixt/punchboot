#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <plat/test/gcov.h>
#include <plat/test/semihosting.h>

static struct gcov_info *head;

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
	//tfp_printf("gcov_init: fn = %s\n", p->filename);
	//tfp_printf("GCOV_COUNTERS = %u\n\r",GCOV_COUNTERS);
	long fd = semihosting_file_open(p->filename, 11);
	
	long f_length = semihosting_file_length(fd);
	//tfp_printf ("  %li %libytes\n\r",fd, f_length);

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

static int counter_active(struct gcov_info *info, unsigned int type)
{
	return info->merge[type] ? 1 : 0;
}

uint32_t gcov_final(void)
{
	size_t bytes_to_write = 0;
	uint32_t a = GCOV_DATA_MAGIC;
	struct gcov_fn_info *fi_ptr;
	struct gcov_ctr_info *ci_ptr;
	unsigned int fi_idx;
	unsigned int ct_idx;
	unsigned int cv_idx;

	struct gcov_info *info = head;

	for (;info; info = info->next) 
	{
		long fd = semihosting_file_open(info->filename, 6);
	
		//tfp_printf ("fn: %s, %u\n\r",info->filename,info->n_functions);
	
		
		bytes_to_write = sizeof(uint32_t);
		a = GCOV_DATA_MAGIC;
		semihosting_file_write(fd, &bytes_to_write, 
								(const uintptr_t) &a);	

		bytes_to_write = sizeof(uint32_t);
		semihosting_file_write(fd, &bytes_to_write, 
								(const uintptr_t) &info->version);	

		bytes_to_write = sizeof(uint32_t);
		semihosting_file_write(fd, &bytes_to_write, 
								(const uintptr_t) &info->stamp);	

		for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) 
		{
			fi_ptr = info->functions[fi_idx];

			//LOG_INFO("fi_ptr->key->filename %s",fi_ptr->key->filename);
			a = GCOV_TAG_FUNCTION;
			bytes_to_write = 4;
			semihosting_file_write(fd, &bytes_to_write, 
									(const uintptr_t) &a);	

			a = GCOV_TAG_FUNCTION_LENGTH;
			bytes_to_write = 4;
			semihosting_file_write(fd, &bytes_to_write, 
									(const uintptr_t) &a);	

			bytes_to_write = 4;
			semihosting_file_write(fd, &bytes_to_write, 
									(const uintptr_t) &fi_ptr->ident);	

			bytes_to_write = 4;
			semihosting_file_write(fd, &bytes_to_write, 
									(const uintptr_t) &fi_ptr->lineno_checksum);



			bytes_to_write = 4;
			semihosting_file_write(fd, &bytes_to_write, 
									(const uintptr_t) &fi_ptr->cfg_checksum);

			//LOG_INFO("fi_idx = %u, lineno_checksum = %8.8X",fi_idx,fi_ptr->lineno_checksum);
			ci_ptr = fi_ptr->ctrs;

			for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
				if (!counter_active(info, ct_idx))
					continue;
				
				//LOG_INFO("%u",ct_idx);
				bytes_to_write = 4;
				a = GCOV_TAG_FOR_COUNTER(ct_idx);
				semihosting_file_write(fd, &bytes_to_write, 
										(const uintptr_t) &a);	

				bytes_to_write = 4;
				a = ci_ptr->num*2;
				semihosting_file_write(fd, &bytes_to_write, 
										(const uintptr_t) &a);	

				//LOG_INFO(" %u ci_ptr->num = %u",ct_idx, ci_ptr->num);
				for (cv_idx = 0; cv_idx < ci_ptr->num; cv_idx++) {
					uint64_t val = ci_ptr->values[cv_idx];
					//LOG_INFO("cv_idx = %u",cv_idx);
					bytes_to_write = 8;
					semihosting_file_write(fd, &bytes_to_write, 
											(const uintptr_t) &val);	
				}

				ci_ptr++;
			}
		}

		semihosting_file_close(fd);
	}

    return 0; /* TODO: What should the return value be*/
}


