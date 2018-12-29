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


static void gcov_write_u32(long fd, uint32_t val)
{
    size_t bytes_to_write = sizeof(uint32_t);

    semihosting_file_write(fd, &bytes_to_write, 
                            (const uintptr_t) &val);	
}

static void gcov_write_u64(long fd, uint64_t val)
{
    size_t bytes_to_write = sizeof(uint64_t);

    semihosting_file_write(fd, &bytes_to_write, 
                            (const uintptr_t) &val);	
}


static uint32_t gcov_read_u32(long fd)
{
    size_t bytes_to_write = sizeof(uint32_t);
    uint32_t val;

    semihosting_file_read(fd, &bytes_to_write, 
                            (const uintptr_t) &val);	

    return val;
}

static uint64_t gcov_read_u64(long fd)
{
    size_t bytes_to_write = sizeof(uint64_t);
    uint64_t val;

    semihosting_file_read(fd, &bytes_to_write, 
                            (const uintptr_t) &val);	

    return val;
}


static int counter_active(struct gcov_info *info, unsigned int type)
{
	return info->merge[type] ? 1 : 0;
}


static void gcov_update_counter(struct gcov_info *info,
                                uint32_t f_ident,
                                uint32_t ctr_tag,
                                uint32_t ctr_idx,
                                uint64_t val)

{
	struct gcov_fn_info *fi_ptr;
	struct gcov_ctr_info *ci_ptr;
	unsigned int fi_idx;
	unsigned int ct_idx;
	unsigned int cv_idx;

    for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) 
    {
        fi_ptr = info->functions[fi_idx];
        ci_ptr = fi_ptr->ctrs;

        if (fi_ptr->ident != f_ident)
            continue;

        for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
            if (!counter_active(info, ct_idx))
                continue;

            if (GCOV_TAG_FOR_COUNTER(ct_idx) ==
                    GCOV_TAG_FOR_COUNTER(ctr_idx))
            {
                //LOG_INFO("Updating %s, %lu", info->filename, ctr_idx);
                ci_ptr->values[ctr_idx] += val;
            }


            ci_ptr++;
        }
    }

}

static void gcov_load_data(struct gcov_info *info)
{
	struct gcov_fn_info *fi_ptr;
	struct gcov_ctr_info *ci_ptr;
	unsigned int fi_idx;
	unsigned int ct_idx;
	unsigned int cv_idx;
    uint32_t dummy;

    long fd = semihosting_file_open(info->filename, 0);

    if (fd < 0)
        return;


    if (gcov_read_u32(fd) != GCOV_DATA_MAGIC)
    {
        LOG_ERR("Invalid magic");
        goto gcov_init_err;
    } 

    if (gcov_read_u32(fd) != info->version)
    {
        LOG_ERR("Incorrect version");
        goto gcov_init_err;
    }

    dummy = gcov_read_u32(fd);

    while(true)
    {
        if (gcov_read_u32(fd) != GCOV_TAG_FUNCTION)
        {
            goto gcov_init_err;
        }

        if (gcov_read_u32(fd) != GCOV_TAG_FUNCTION_LENGTH)
        {
            LOG_ERR("Invalid function length tag");
            goto gcov_init_err;
        }

        uint32_t f_ident = gcov_read_u32(fd);
        dummy = gcov_read_u32(fd);
        dummy = gcov_read_u32(fd);

        UNUSED(dummy);

        for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) 
        {
			if (!counter_active(info, ct_idx))
				continue;

            uint32_t ctr_tag = gcov_read_u32(fd);
            uint32_t n = gcov_read_u32(fd);

            for (cv_idx = 0; cv_idx < n; cv_idx++) 
            {
                uint64_t val = gcov_read_u64(fd);
                gcov_update_counter(info, f_ident, ctr_tag, cv_idx, val);
            }

        }
    }


    LOG_INFO(" OK");
gcov_init_err:

    semihosting_file_close(fd);
}

void __gcov_init (struct gcov_info *p)
{

	p->next = head;
	head = p;
 
    gcov_load_data(p);

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
	struct gcov_fn_info *fi_ptr;
	struct gcov_ctr_info *ci_ptr;
	unsigned int fi_idx;
	unsigned int ct_idx;
	unsigned int cv_idx;

	struct gcov_info *info = head;

	for (;info; info = info->next) 
	{

		long fd = semihosting_file_open(info->filename, 6);

        gcov_write_u32(fd, GCOV_DATA_MAGIC);
        gcov_write_u32(fd, info->version);
        gcov_write_u32(fd, info->stamp);

		for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) 
		{
			fi_ptr = info->functions[fi_idx];

            gcov_write_u32(fd, GCOV_TAG_FUNCTION);
            gcov_write_u32(fd, GCOV_TAG_FUNCTION_LENGTH);
            gcov_write_u32(fd, fi_ptr->ident);
            gcov_write_u32(fd, fi_ptr->lineno_checksum);
            gcov_write_u32(fd, fi_ptr->cfg_checksum);

			ci_ptr = fi_ptr->ctrs;

			for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
				if (!counter_active(info, ct_idx))
					continue;
	
                gcov_write_u32(fd, GCOV_TAG_FOR_COUNTER(ct_idx));
                gcov_write_u32(fd, ci_ptr->num * 2);

				for (cv_idx = 0; cv_idx < ci_ptr->num; cv_idx++) 
                    gcov_write_u64(fd, ci_ptr->values[cv_idx]);

				ci_ptr++;
			}
		}

		semihosting_file_close(fd);
	}

    return 0; /* TODO: What should the return value be*/
}


