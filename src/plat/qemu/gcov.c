/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/plat.h>
#include <plat/qemu/gcov.h>
#include <plat/qemu/semihosting.h>

static struct gcov_info *head;

#define GCOV_TAG_FUNCTION_LENGTH    3

#define GCOV_DATA_MAGIC     ((uint32_t) 0x67636461)
#define GCOV_TAG_FUNCTION   ((uint32_t) 0x01000000)
#define GCOV_TAG_COUNTER_BASE   ((uint32_t) 0x01a10000)
#define GCOV_TAG_FOR_COUNTER(count)                 \
        (GCOV_TAG_COUNTER_BASE + ((uint32_t) (count) << 17))


static int gcov_write_u32(long fd, uint32_t val)
{
    int rc;
    size_t bytes_to_write = sizeof(uint32_t);

    rc = semihosting_file_write(fd, &bytes_to_write, (const uintptr_t) &val);

    if (rc != 0 || bytes_to_write != 0)
        return -1;
    else
        return 0;
}

static int gcov_write_u64(long fd, uint64_t val)
{
    int rc;
    size_t bytes_to_write = sizeof(uint64_t);

    rc = semihosting_file_write(fd, &bytes_to_write, (const uintptr_t) &val);

    if (rc != 0 || bytes_to_write != 0)
        return -1;
    else
        return 0;
}


static int gcov_read_u32(long fd, uint32_t *output)
{
    int rc;
    size_t bytes_to_read = sizeof(uint32_t);

    rc = semihosting_file_read(fd, &bytes_to_read, (const uintptr_t) output);

    if (rc != 0 || bytes_to_read != sizeof(uint32_t))
        return -1;
    else
        return 0;
}

static int gcov_read_u64(long fd, uint64_t *output)
{
    int rc;
    size_t bytes_to_read = sizeof(uint64_t);

    rc = semihosting_file_read(fd, &bytes_to_read, (const uintptr_t) output);

    if (rc != 0 || bytes_to_read != sizeof(uint64_t))
        return -1;
    else
        return 0;
}


static int counter_active(struct gcov_info *info, unsigned int type)
{
    return info->merge[type] ? 1 : 0;
}

static void gcov_update_counter(struct gcov_info *info,
                                uint32_t num,
                                uint32_t f_ident,
                                uint32_t ctr_tag,
                                uint64_t val)

{
    struct gcov_fn_info *fi_ptr;
    struct gcov_ctr_info *ci_ptr;
    unsigned int fi_idx;
    unsigned int ct_idx;

    for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) {
        fi_ptr = info->functions[fi_idx];
        ci_ptr = fi_ptr->ctrs;

        if (fi_ptr->ident != f_ident)
            continue;
        for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
            if (GCOV_TAG_FOR_COUNTER(ct_idx) == ctr_tag) {
                ci_ptr->values[num] += val;
            }
            ci_ptr++;
        }
    }
}

static int gcov_load_data(struct gcov_info *info)
{
    int rc = 0;
    unsigned int ct_idx;
    uint32_t val;
    uint32_t f_ident;

    long fd = semihosting_file_open(info->filename, 0);

    if (fd == -1) {
        /* When we come here for the first time the file does not exist,
         * thus -1 is okay */
        return 0;
    } else if (fd < -1) {
        LOG_ERR("Can't open '%s' %i", info->filename, rc);
        return fd;
    }

    rc = gcov_read_u32(fd, &val);

    if (val != GCOV_DATA_MAGIC || rc != 0) {
        LOG_ERR("Invalid magic %x, %i", val, rc);
        rc = -2;
        goto gcov_init_err;
    }

    rc = gcov_read_u32(fd, &val);

    if (val != info->version || rc != 0) {
        LOG_ERR("Incorrect version %x, %i", val, rc);
        rc = -3;
        goto gcov_init_err;
    }

    rc = gcov_read_u32(fd, &val); // Ignore 'stamp'

    if (rc != 0) {
        LOG_ERR("Could not read stamp field (%i)", rc);
        rc = -4;
        goto gcov_init_err;
    }

    for (unsigned int i = 0; i < info->n_functions; i++) {
        rc = gcov_read_u32(fd, &val);

        if (val != GCOV_TAG_FUNCTION || rc != 0) {
            LOG_ERR("Invalid tag %x, %i", val, rc);
            rc = -5;
            goto gcov_init_err;
        }

        rc = gcov_read_u32(fd, &val);

        if (val != GCOV_TAG_FUNCTION_LENGTH || rc != 0) {
            LOG_ERR("Invalid function length tag %x, %i", val, rc);
            rc = -6;
            goto gcov_init_err;
        }

        rc = gcov_read_u32(fd, &f_ident);

        if (rc != 0) {
            LOG_ERR("Could not read ident field (%i)", rc);
            rc = -7;
            goto gcov_init_err;
        }

        rc = gcov_read_u32(fd, &val); // Ignore lineno checksum
        if (rc != 0) {
            LOG_ERR("Could not read lineno checksum (%i)", rc);
            rc = -8;
            goto gcov_init_err;
        }

        rc = gcov_read_u32(fd, &val); // Ignore cfg checksum

        if (rc != 0) {
            LOG_ERR("Could not read cfg checksum (%i)", rc);
            rc = -9;
            goto gcov_init_err;
        }

        for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
            if (!counter_active(info, ct_idx))
                continue;

            uint32_t ctr_tag;
            uint32_t num;

            rc = gcov_read_u32(fd, &ctr_tag);

            if (rc != 0) {
                LOG_ERR("Could not read ctr tag (%i)", rc);
                goto gcov_init_err;
            }

            rc = gcov_read_u32(fd, &num);

            if (rc != 0) {
                LOG_ERR("Could not read num tag (%i)", rc);
                goto gcov_init_err;
            }

            for (unsigned int n = 0; n < (num / 2); n++) {
                uint64_t counter_val;
                rc = gcov_read_u64(fd, &counter_val);

                if (rc != 0) {
                    LOG_ERR("Could not read value (%i)", rc);
                    goto gcov_init_err;
                }

                gcov_update_counter(info, n, f_ident, ctr_tag, counter_val);
            }
        }
    }

gcov_init_err:
    semihosting_file_close(fd);
    if (rc < 0) {
        LOG_ERR("File was: '%s'", info->filename);
    }
    return rc;
}

void __gcov_init(struct gcov_info *p); /* Suppress build warning */
void __gcov_init(struct gcov_info *p)
{
    p->next = head;
    head = p;
}

void __gcov_merge_add(gcov_type *counters, unsigned n_counters);
void __gcov_merge_add(gcov_type *counters, unsigned n_counters)
{
    (void) counters;
    (void) n_counters;

    /* Not Used */
}

void __gcov_flush(void);
void __gcov_flush(void)
{
    /* Not used */
}

void __gcov_exit(void);
void __gcov_exit(void)
{
    /* Not used */
}

void gcov_init(void)
{
    /* Call gcov initalizers */
    extern uint32_t __init_array_start, __init_array_end;
    void (**pctor)(void) = (void (**)(void)) &__init_array_start;
    void (**pctor_last)(void) = (void (**)(void)) &__init_array_end;

    for (; pctor < pctor_last; pctor++) {
        (*pctor)();
    }
}

int gcov_store_output(void)
{
    int rc;
    struct gcov_fn_info *fi_ptr;
    struct gcov_ctr_info *ci_ptr;
    unsigned int fi_idx;
    unsigned int ct_idx;
    unsigned int cv_idx;

    struct gcov_info *info = head;

    for (; info; info = info->next) {
        rc = gcov_load_data(info);

        if (rc < 0) {
            LOG_ERR("load error (%i)", rc);
            return rc;
        }

        long fd = semihosting_file_open(info->filename, 6);

        if (fd < 0) {
            LOG_ERR("Could not open %s", info->filename);
            return fd;
        }

        rc = gcov_write_u32(fd, GCOV_DATA_MAGIC);
        if (rc != 0)
            return rc;

        rc = gcov_write_u32(fd, info->version);
        if (rc != 0)
            return rc;

        rc = gcov_write_u32(fd, info->stamp);
        if (rc != 0)
            return rc;

        for (fi_idx = 0; fi_idx < info->n_functions; fi_idx++) {
            fi_ptr = info->functions[fi_idx];
            rc = gcov_write_u32(fd, GCOV_TAG_FUNCTION);
            if (rc != 0)
                return rc;
            rc = gcov_write_u32(fd, GCOV_TAG_FUNCTION_LENGTH);
            if (rc != 0)
                return rc;
            rc = gcov_write_u32(fd, fi_ptr->ident);
            if (rc != 0)
                return rc;
            rc = gcov_write_u32(fd, fi_ptr->lineno_checksum);
            if (rc != 0)
                return rc;
            rc = gcov_write_u32(fd, fi_ptr->cfg_checksum);
            if (rc != 0)
                return rc;

            ci_ptr = fi_ptr->ctrs;

            for (ct_idx = 0; ct_idx < GCOV_COUNTERS; ct_idx++) {
                if (!counter_active(info, ct_idx))
                    continue;

                rc = gcov_write_u32(fd, GCOV_TAG_FOR_COUNTER(ct_idx));
                if (rc != 0)
                    return rc;
                rc = gcov_write_u32(fd, ci_ptr->num * 2);
                if (rc != 0)
                    return rc;

                for (cv_idx = 0; cv_idx < ci_ptr->num; cv_idx++) {
                    rc = gcov_write_u64(fd, ci_ptr->values[cv_idx]);
                    if (rc != 0)
                        return rc;
                }

                ci_ptr++;
            }
        }

        semihosting_file_close(fd);
    }

    return 0;
}
