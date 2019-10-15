
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <board.h>
#include <io.h>
#include <plat.h>
#include <string.h>
#include <uuid.h>
#include <fuse.h>
#include <boot.h>
#include <plat/test/virtio.h>
#include <plat/test/virtio_block.h>
#include <plat/test/test_fuse.h>
#include <plat/test/gcov.h>
#include <plat/test/uart.h>
#include <3pp/bearssl/bearssl_hash.h>
#include <plat/test/semihosting.h>

static __a4k struct virtio_block_device virtio_block;
static __a4k struct virtio_block_device virtio_block2;
static struct virtio_block_device *blk;
static struct pb_platform_setup setup;

static uint32_t blk_off = 0;
static uint32_t blk_sz = 65535;
static uint32_t setup_locked = 0;
extern const struct fuse fuses[];

void pb_boot(struct pb_pbi *pbi, uint32_t active_system, bool verbose)
{
    UNUSED(pbi);
    UNUSED(active_system);
    UNUSED(verbose);

    long fd = semihosting_file_open("/tmp/pb_boot_status", 6);

    char boot_status[5];

    snprintf(boot_status, sizeof(boot_status), "%u", active_system);

    size_t bytes_to_write = strlen(boot_status);

    semihosting_file_write(fd, &bytes_to_write,
                            (const uintptr_t) boot_status);

    semihosting_file_close(fd);
    plat_reset();
}

__inline uint32_t plat_get_ms_tick(void)
{
    return 1;
}


uint32_t plat_get_security_state(uint32_t *state)
{
    uint32_t err;
    (*state) = PB_SECURITY_STATE_NOT_SECURE;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);

        if (f->value)
        {
            (*state) = PB_SECURITY_STATE_CONFIGURED_ERR;
            break;
        }

        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    /* Perform checks to see if the boot was successful */
    /* for the test platform there is no point in testing
     *  this => CONFIGURED_OK */
    (*state) = PB_SECURITY_STATE_CONFIGURED_OK;

    if (setup_locked)
        (*state) = PB_SECURITY_STATE_SECURE;

    return PB_OK;
}

uint32_t plat_get_params(struct param **pp)
{
    char uuid_raw[16];

    param_add_str((*pp)++, "Platform", "Test");
    param_add_str((*pp)++, "Machine", "QEMU, Cortex-A15");
    plat_get_uuid(uuid_raw);
    param_add_uuid((*pp)++, "Device UUID", uuid_raw);
    return PB_OK;
}

static const char *platform_namespace_uuid =
    "\x3f\xaf\xc6\xd3\xc3\x42\x4e\xdf\xa5\xa6\x0e\xb1\x39\xa7\x83\xb5";

static const char *device_unique_id =
    "\xbe\x4e\xfc\xb4\x32\x58\xcd\x63";

uint32_t plat_get_uuid(char *out)
{
    return uuid_gen_uuid3(platform_namespace_uuid, 16,
                          (const char *) device_unique_id, 8, out);
}

uint32_t plat_setup_device(struct param *params)
{
    uint32_t err;

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) fuses)
    {
        err = plat_fuse_read(f);

        LOG_DBG("Fuse %s: 0x%08x", f->description, f->value);
        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'", f->description);
            return err;
        }
    }

    /* Perform the actual fuse programming */

    LOG_INFO("Writing fuses");

    foreach_fuse(f, fuses)
    {
        f->value = f->default_value;
        err = plat_fuse_write(f);

        if (err != PB_OK)
            return err;
    }

    return board_setup_device(params);
}


uint32_t plat_setup_lock(void)
{
    if (setup_locked)
        return PB_ERR;
    setup_locked = 1;
    return PB_OK;
}

bool plat_force_recovery(void)
{
    return board_force_recovery(&setup);
}

uint32_t plat_prepare_recovery(void)
{
    return board_prepare_recovery(&setup);
}

uint32_t plat_fuse_read(struct fuse *f)
{
    uint32_t tmp_val = 0;
    uint32_t err;

    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return PB_ERR;

    err = test_fuse_read(f->bank, &tmp_val);

    if (err != PB_OK)
        LOG_ERR("Could not read fuse");

    f->value = tmp_val;

    return err;
}

uint32_t plat_fuse_write(struct fuse *f)
{
    uint32_t err;

    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return PB_ERR;
    LOG_DBG("Writing fuse %s", f->description);

    err = test_fuse_write(&virtio_block2, f->bank, f->value);

    if (err != PB_OK)
        LOG_ERR("Could not write fuse");
    return err;
}

uint32_t plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return PB_ERR;

    return snprintf(s, n, "FUSE <%u> %s = 0x%08x\n", f->bank,
                f->description, f->value);
}

uint32_t plat_early_init(void)
{
    board_early_init(NULL);

    test_uart_init();
    LOG_INFO("Plat start");
    gcov_init();

    virtio_block.dev.device_id = 2;
    virtio_block.dev.vendor_id = 0x554D4551;
    virtio_block.dev.base = 0x0A003C00;

    if (virtio_block_init(&virtio_block) != PB_OK)
    {
        LOG_ERR("Could not initialize virtio block device");
        while (1) {}
    }

    blk = &virtio_block;

    virtio_block2.dev.device_id = 2;
    virtio_block2.dev.vendor_id = 0x554D4551;
    virtio_block2.dev.base = 0x0A003A00;

    if (virtio_block_init(&virtio_block2) != PB_OK)
    {
        LOG_ERR("Could not initialize virtio block device");
        while (1) {}
    }

    test_fuse_init(&virtio_block2);


    board_late_init(NULL);
    return PB_OK;
}

void plat_preboot_cleanup(void)
{
}

uint32_t plat_get_us_tick(void)
{
    return 0;
}


uint32_t  plat_write_block(uint32_t lba_offset,
                                uintptr_t bfr,
                                uint32_t no_of_blocks)
{
    return virtio_block_write(blk, (blk_off+lba_offset),
                                bfr, no_of_blocks);
}

uint32_t  plat_write_block_async(uint32_t lba_offset,
                                uintptr_t bfr,
                                uint32_t no_of_blocks)
{
    return virtio_block_write(blk, (blk_off+lba_offset),
                                bfr, no_of_blocks);
}

uint32_t plat_flush_block(void)
{
    return PB_OK;
}

uint32_t  plat_read_block(uint32_t lba_offset,
                          uintptr_t bfr,
                          uint32_t no_of_blocks)
{
    return virtio_block_read(blk, (blk_off+lba_offset), bfr,
                                no_of_blocks);
}

uint32_t  plat_switch_part(uint8_t part_no)
{
    switch (part_no)
    {
        case PLAT_EMMC_PART_BOOT0:
        {
            blk = &virtio_block2;
            /* First 10 blocks are reserved for emulated fuses */
            blk_off = 10;
            blk_sz = 2048;
            LOG_INFO("Switching to aux disk with offset: %u blks", blk_off);
        }
        break;
        case PLAT_EMMC_PART_BOOT1:
        {
            blk = &virtio_block2;
            blk_off = 10 + 8192;
            blk_sz = 2048;
            LOG_INFO("Switching to aux disk with offset: %u blks", blk_off);
        }
        break;
        case PLAT_EMMC_PART_USER:
        {
            blk = &virtio_block;
            blk_off = 0;
            blk_sz = 65535;
            LOG_INFO("Switching to main disk with offset: %u blks", blk_off);
        }
        break;
        default:
        {
            blk = &virtio_block;
            blk_off = 0;
            blk_sz = 65535;
        }
    }

    return PB_OK;
}

uint64_t  plat_get_lastlba(void)
{
    return blk_sz;
}
