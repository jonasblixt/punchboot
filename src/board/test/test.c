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


struct virtio_serial_device virtio_serial;
struct virtio_block_device virtio_block;

uint8_t heap[8192];
uint32_t heap_ptr = 0;

void *malloc(size_t size)
{
	if ((heap_ptr+size) > 8192)
	{
		LOG_ERR("Out of memory");
		return NULL;
	}
	void * ptr = (void *) heap[heap_ptr];
	heap_ptr += size;

	return ptr;
}

void free(void *ptr)
{
}

void *calloc(size_t nmemb, size_t size)
{
	return malloc (nmemb*size);
}

void *realloc(void *ptr, size_t size)
{
	return malloc(size);
}

int rand(void)
{
	return 0;
}


const char *__locale_ctype_ptr(void)
{
}

uint64_t __aeabi_uldivmod(uint64_t a, uint64_t b)
{
}

void qsort(void *base, size_t nmemb, size_t size,
			int(*compar)(const void*, const void*))
{
}

clock_t clock(void)
{
}

uint32_t board_init(void)
{

    test_uart_init();
    init_printf(NULL, &plat_uart_putc);
 
    pl061_init(0x09030000);

	gcov_init();	

    virtio_serial.dev.device_id = 3;
    virtio_serial.dev.vendor_id = 0x554D4551;
    virtio_serial.dev.base = 0x0A003E00;

    if (virtio_serial_init(&virtio_serial) != PB_OK)
    {
        LOG_ERR("Could not initialize virtio serial port");
        while(1);
    }

    virtio_block.dev.device_id = 2;
    virtio_block.dev.vendor_id = 0x554D4551;
    virtio_block.dev.base = 0x0A003C00;

    if (virtio_block_init(&virtio_block) != PB_OK)
    {
        LOG_ERR("Could not initialize virtio block device");
        while(1);
    }

 /*   uint32_t err = rsa_verify_hash_ex (hash, 0, hash, 32, 
                    LTC_PKCS_1_V1_5 , 0, 0, NULL, NULL);
*/
    return PB_OK;
}


uint32_t  plat_emmc_write_block(uint32_t lba_offset, 
                                uint8_t *bfr, 
                                uint32_t no_of_blocks)
{
	return virtio_block_write(&virtio_block, lba_offset, bfr, no_of_blocks);

}


uint32_t  plat_emmc_read_block( uint32_t lba_offset, 
                                uint8_t *bfr, 
                                uint32_t no_of_blocks)
{
    return virtio_block_read(&virtio_block, lba_offset, bfr, no_of_blocks);
}

uint32_t  plat_emmc_switch_part(uint8_t part_no)
{
    UNUSED(part_no);
    return PB_ERR;
}

uint64_t  plat_emmc_get_lastlba(void)
{
    return 32768;
}

uint8_t board_force_recovery(void) 
{
    return true;
}


uint32_t board_get_uuid(uint8_t *uuid) 
{
    UNUSED(uuid);
    return PB_OK;
}

uint32_t board_get_boardinfo(struct board_info *info) 
{
    UNUSED(info);
    return PB_OK;
}

uint32_t board_write_uuid(uint8_t *uuid, uint32_t key) 
{
    UNUSED(uuid);
    UNUSED(key);

    return PB_OK;
}

uint32_t board_write_boardinfo(struct board_info *info, uint32_t key) 
{
    UNUSED(info);
    UNUSED(key);
    return PB_OK;
}

uint32_t board_write_gpt_tbl() 
{
    gpt_init_tbl(1, plat_emmc_get_lastlba());
    gpt_add_part(0, 1, part_type_config, "Config");
    gpt_add_part(1, 1024, part_type_system_a, "System A");
    gpt_add_part(2, 1024, part_type_system_b, "System B");
    return gpt_write_tbl();
}

uint32_t board_write_standard_fuses(uint32_t key) 
{
    UNUSED(key);
   return PB_OK;
}

uint32_t board_write_mac_addr(uint8_t *mac_addr, 
							  uint32_t len, 
							  uint32_t index, 
							  uint32_t key) 
{
    UNUSED(mac_addr);
    UNUSED(len);
    UNUSED(index);
    UNUSED(key);

    return PB_OK;
}

uint32_t board_enable_secure_boot(uint32_t key) 
{
    UNUSED(key);
    return PB_OK;
}

void board_boot(struct pb_pbi *pbi)
{
    UNUSED(pbi);
	LOG_INFO("Booting");
	while (1);
}
