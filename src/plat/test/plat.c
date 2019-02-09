#include <stdio.h>
#include <board.h>
#include <io.h>
#include <plat.h>
#include <string.h>
#include <fuse.h>
#include <plat/test/virtio.h>
#include <plat/test/virtio_block.h>
#include <plat/test/test_fuse.h>
#include <3pp/bearssl/bearssl_hash.h>

static __a4k struct virtio_block_device virtio_block;
static __a4k struct virtio_block_device virtio_block2;
static struct virtio_block_device *blk = &virtio_block;

uint32_t blk_off = 0;
uint32_t blk_sz = 65535;

__inline uint32_t plat_get_ms_tick(void) 
{
    return 1;
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
    LOG_DBG ("Writing fuse %s",f->description);

    err = test_fuse_write(&virtio_block2,f->bank, f->value);

    if (err != PB_OK)
        LOG_ERR("Could not write fuse");
    return err;
}

uint32_t plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return PB_ERR;

    return snprintf (s,n,"FUSE <%u> %s = 0x%08x\n", f->bank, 
                f->description, f->value);
}

uint32_t plat_early_init(void)
{
    board_early_init();


    virtio_block.dev.device_id = 2;
    virtio_block.dev.vendor_id = 0x554D4551;
    virtio_block.dev.base = 0x0A003C00;

    if (virtio_block_init(&virtio_block) != PB_OK)
    {
        LOG_ERR("Could not initialize virtio block device");
        while(1);
    }

    virtio_block2.dev.device_id = 2;
    virtio_block2.dev.vendor_id = 0x554D4551;
    virtio_block2.dev.base = 0x0A003A00;

    if (virtio_block_init(&virtio_block2) != PB_OK)
    {
        LOG_ERR("Could not initialize virtio block device");
        while(1);
    }

    test_fuse_init(&virtio_block2);

    return PB_OK;

}

uint32_t plat_get_us_tick(void)
{
    return 0;
}


uint32_t  plat_write_block(uint32_t lba_offset, 
                                uintptr_t bfr, 
                                uint32_t no_of_blocks)
{
	return virtio_block_write(blk, blk_off+lba_offset, 
                                (uint8_t *)bfr, no_of_blocks);

}


uint32_t  plat_read_block( uint32_t lba_offset, 
                                uintptr_t bfr, 
                                uint32_t no_of_blocks)
{
    return virtio_block_read(blk, blk_off+lba_offset, (uint8_t *)bfr, 
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
