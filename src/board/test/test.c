/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <io.h>
#include <gpt.h>
#include <usb.h>
#include <tomcrypt.h>

#include <plat/test/uart.h>
#include <plat/test/pl061.h>
#include <plat/test/virtio.h>
#include <plat/test/virtio_serial.h>
#include <plat/test/virtio_block.h>
#include <plat/test/gcov.h>

#include "board_config.h"

const uint8_t part_type_config[] = {0xF7, 0xDD, 0x45, 0x34, 0xCC, 0xA5, 0xC6, 
        0x45, 0xAA, 0x17, 0xE4, 0x10, 0xA5, 0x42, 0xBD, 0xB8};

const uint8_t part_type_system_a[] = {0x59, 0x04, 0x49, 0x1E, 0x6D, 0xE8, 0x4B, 
        0x44, 0x82, 0x93, 0xD8, 0xAF, 0x0B, 0xB4, 0x38, 0xD1};

const uint8_t part_type_system_b[] = { 0x3C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 
        0x42, 0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04,};


struct test_fuse_map
{
    char uuid[16];
    struct board_info binfo;
    uint8_t _rz[512];
} __attribute__ ((packed)) __a4k _fuses;

static struct usb_device usbdev =
{
    .platform_data = NULL,
};

uint32_t board_usb_init(struct usb_device **dev)
{
	*dev = &usbdev;
	return PB_OK;
}


__inline uint32_t plat_get_ms_tick(void) 
{
    return 1;
}

struct virtio_block_device virtio_block;
struct virtio_block_device virtio_block2;

struct virtio_block_device *blk = &virtio_block;
uint32_t blk_off = 0;
uint32_t blk_sz = 65535;

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



uint32_t plat_get_uuid(uint8_t *uuid) 
{
    virtio_block_read(&virtio_block2,0,  (uint8_t *)&_fuses, 1);

    memcpy(uuid,_fuses.uuid,16);
    return PB_OK;
}

uint32_t plat_write_uuid(uint8_t *uuid, uint32_t key) 
{
    UNUSED(key);

    virtio_block_read(&virtio_block2,0,  (uint8_t *)&_fuses, 1);

    for (uint32_t n = 0; n < 16; n++)
    {
        if (_fuses.uuid[n] != 0)
            return PB_ERR;
    }

    memcpy(_fuses.uuid,uuid,16);
    virtio_block_write(&virtio_block2, 0, (uint8_t *) &_fuses, 1);
    return PB_OK;
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

uint32_t board_init(void)
{

    test_uart_init();
    init_printf(NULL, &plat_uart_putc);
 
    pl061_init(0x09030000);

	gcov_init();	


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

    virtio_block_read(&virtio_block2,0,  (uint8_t *)&_fuses, 1);

    return PB_OK;
}


uint32_t  plat_write_block(uint32_t lba_offset, 
                                uint8_t *bfr, 
                                uint32_t no_of_blocks)
{
	return virtio_block_write(blk, blk_off+lba_offset, bfr, no_of_blocks);

}


uint32_t  plat_read_block( uint32_t lba_offset, 
                                uint8_t *bfr, 
                                uint32_t no_of_blocks)
{
    return virtio_block_read(blk, blk_off+lba_offset, bfr, no_of_blocks);
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
            LOG_INFO("Switching to aux disk with offset: %lu blks", blk_off);
        }
        break;
        case PLAT_EMMC_PART_BOOT1:
        {
            blk = &virtio_block2;
            blk_off = 10 + 8192;
            blk_sz = 2048;
            LOG_INFO("Switching to aux disk with offset: %lu blks", blk_off);
        }
        break;
        case PLAT_EMMC_PART_USER:
        {
            blk = &virtio_block;
            blk_off = 0;
            blk_sz = 65535;
            LOG_INFO("Switching to main disk with offset: %lu blks", blk_off);
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

uint8_t board_force_recovery(void) 
{
    return true;
}



uint32_t board_get_boardinfo(struct board_info *info) 
{
    virtio_block_read(&virtio_block2,0,  (uint8_t *)&_fuses, 1);
    memcpy(info, &_fuses.binfo, sizeof(struct board_info));

    return PB_OK;
}

uint32_t board_write_boardinfo(struct board_info *info, uint32_t key) 
{
    UNUSED(key);
    memcpy(&_fuses.binfo, info, sizeof(struct board_info));
    virtio_block_write(&virtio_block2, 0, (uint8_t *) &_fuses, 1);
    return PB_OK;
}

uint32_t board_write_gpt_tbl() 
{
    gpt_add_part(1, 1024, part_type_system_a, "System A");
    gpt_add_part(2, 1024, part_type_system_b, "System B");
    return PB_OK;
}

uint32_t board_write_standard_fuses(uint32_t key) 
{
    UNUSED(key);
   return PB_OK;
}


void test_board_boot(struct pb_pbi *pbi)
{

    struct pb_component_hdr *linux = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_LINUX);

	LOG_INFO("Booting linux image at addr %8.8lX",linux->load_addr_low);

    plat_reset();
}
