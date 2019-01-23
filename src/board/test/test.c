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
#include <plat/test/plat.h>

#include <board/config.h>

const uint8_t part_type_config[] = 
{
    0xF7, 0xDD, 0x45, 0x34, 0xCC, 0xA5, 0xC6, 0x45, 
    0xAA, 0x17, 0xE4, 0x10, 0xA5, 0x42, 0xBD, 0xB8
};

const uint8_t part_type_system_a[] = 
{
    0x59, 0x04, 0x49, 0x1E, 0x6D, 0xE8, 0x4B, 0x44, 
    0x82, 0x93, 0xD8, 0xAF, 0x0B, 0xB4, 0x38, 0xD1
};

const uint8_t part_type_system_b[] = 
{ 
    0x3C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};

const uint8_t part_type_root_a[] = 
{ 
    0x1C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};

const uint8_t part_type_root_b[] = 
{ 
    0x2C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};


const struct fuse uuid_fuses[] =
{
    TEST_FUSE_BANK_WORD(0, "UUID0"),
    TEST_FUSE_BANK_WORD(1, "UUID1"),
    TEST_FUSE_BANK_WORD(2, "UUID2"),
    TEST_FUSE_BANK_WORD(3, "UUID3"),
    TEST_FUSE_END,
};

const struct fuse device_info_fuses[] =
{
    TEST_FUSE_BANK_WORD_VAL(4, "Device Info", 0x12340000),
    TEST_FUSE_END,
};

const struct fuse root_hash_fuses[] =
{
    TEST_FUSE_BANK_WORD(5, "SRK0"),
    TEST_FUSE_BANK_WORD(6, "SRK1"),
    TEST_FUSE_BANK_WORD(7, "SRK2"),
    TEST_FUSE_BANK_WORD(8, "SRK3"),
    TEST_FUSE_BANK_WORD(9, "SRK4"),
    TEST_FUSE_BANK_WORD(10, "SRK5"),
    TEST_FUSE_BANK_WORD(11, "SRK6"),
    TEST_FUSE_BANK_WORD(12, "SRK7"),
    TEST_FUSE_END,
};


const struct fuse board_fuses[] =
{
    TEST_FUSE_BANK_WORD_VAL(13, "BOOT0", 0x12341234),
    TEST_FUSE_BANK_WORD_VAL(14, "BOOT1", 0xCAFEEFEE),
    TEST_FUSE_END,
};


static struct usb_device usbdev =
{
    .platform_data = NULL,
};

uint32_t board_usb_init(struct usb_device **dev)
{
	*dev = &usbdev;
	return PB_OK;
}

uint32_t board_early_init(void)
{

    test_uart_init();
    init_printf(NULL, &plat_uart_putc);
 
    pl061_init(0x09030000);

	gcov_init();	

    return PB_OK;
}

uint8_t board_force_recovery(void) 
{
    return true;
}

uint32_t board_configure_gpt_tbl() 
{
    gpt_add_part(1, 1024, part_type_system_a, "System A");
    gpt_add_part(2, 1024, part_type_system_b, "System B");
    return PB_OK;
}

void test_board_boot(struct pb_pbi *pbi, uint8_t system_index)
{
    UNUSED(system_index);
    struct pb_component_hdr *linux = 
            pb_image_get_component(pbi, PB_IMAGE_COMPTYPE_LINUX);

	LOG_INFO("Booting linux image at addr %8.8lX",linux->load_addr_low);

    plat_reset();
}

uint32_t board_configure_bootargs(char *buf, char *boot_part_uuid)
{
    UNUSED(buf);
    UNUSED(boot_part_uuid);
    return PB_OK;
}
