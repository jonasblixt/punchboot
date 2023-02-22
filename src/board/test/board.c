/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/board.h>
#include <pb/fuse.h>
#include <pb/plat.h>
#include <pb/storage.h>
#include <pb/gpt.h>
#include <pb/boot.h>
#include <plat/qemu/plat.h>
#include <plat/qemu/uart.h>
#include <plat/qemu/semihosting.h>
#include <plat/qemu/virtio_block.h>
#include <plat/qemu/virtio_serial.h>
#include <libfdt.h>
#include <uuid.h>

struct fuse fuses[] =
{
    TEST_FUSE_BANK_WORD_VAL(0,  "SRK0",  0x5020C7D7),
    TEST_FUSE_BANK_WORD_VAL(1,  "SRK1",  0xBB62B945),
    TEST_FUSE_BANK_WORD_VAL(2,  "SRK2",  0xDD97C8BE),
    TEST_FUSE_BANK_WORD_VAL(3,  "SRK3",  0xDC6710DD),
    TEST_FUSE_BANK_WORD_VAL(4,  "SRK4",  0x2756B777),
    TEST_FUSE_BANK_WORD_VAL(5, "BOOT0", 0x12341234),
    TEST_FUSE_BANK_WORD_VAL(6, "BOOT1", 0xCAFEEFEE),
    TEST_FUSE_END,
};

struct fuse board_ident_fuse =
        TEST_FUSE_BANK_WORD(7, "Ident");


const uint32_t rom_key_map[] =
{
    0xa90f9680,
    0x25c6dd36,
    0x52c1eda0,
    0xcca57803,
    0x00000000,
};

#define DEF_FLAGS (PB_STORAGE_MAP_FLAG_WRITABLE | \
                   PB_STORAGE_MAP_FLAG_VISIBLE)

struct pb_storage_map map[] =
{

    PB_STORAGE_MAP("2af755d8-8de5-45d5-a862-014cfa735ce0", "System A", 1024,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("c046ccd8-0f2e-4036-984d-76c14dc73992", "System B", 1024,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("f5f8c9ae-efb5-4071-9ba9-d313b082281e", "PB State Primary",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP("656ab3fc-5856-4a5e-a2ae-5a018313b3ee", "PB State Backup",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP("44acdcbe-dcb0-4d89-b0ad-8f96967f8c95", "Fuse array",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP("ff4ddc6c-ad7a-47e8-8773-6729392dd1b5", "Dumpable", 2048,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_DUMPABLE),

    PB_STORAGE_MAP_END
};

/* storage driver configuration */

static uint8_t gpt_private_data[4096*9] __no_bss __a4k;
static uint8_t map_data[4096*4] __no_bss __a4k;

static struct virtio_block_device virtio_block __a4k =
{
    .dev =
    {
        .device_id = 2,
        .vendor_id = 0x554D4551,
        .base = 0x0A003C00,
    },
};

static struct pb_storage_driver virtio_driver =
{
    .name = "virtio0",
    .block_size = 512,
    .driver_private = &virtio_block,
    .last_block = (CONFIG_QEMU_VIRTIO_DISK_SIZE_MB*2048-1),
    .init = virtio_block_init,
    .read = virtio_block_read,
    .write = virtio_block_write,

    .map_default = map,
    .map_init = gpt_init,
    .map_install = gpt_install_map,
    .map_resize = gpt_resize_map,
    .map_data = map_data,
    .map_data_size = sizeof(map_data),
    .map_private = gpt_private_data,
    .map_private_size = sizeof(gpt_private_data),
};

/* END of storage */

/* Board specific */


bool board_force_command_mode(void *plat)
{
    long fd = semihosting_file_open("/tmp/pb_force_command_mode", 0);

    LOG_DBG("Checking for /tmp/pb_force_command_mode");

    if (fd != -1)
    {
        semihosting_file_close(fd);
        return true;
    }

    return false;
}


int board_command(void *plat,
                     uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size)
{
    size_t resp_buf_size = *response_size;
    char *response = (char *) response_bfr;

    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xc72b6e9e) /* test-command */
    {
        char *arg = (char *) bfr;
        arg[size] = 0;

        LOG_DBG("test-command (%s)", arg);

        (*response_size) = snprintf(response, resp_buf_size,
                                "Hello test-command: %s\n", arg);

        return PB_OK;
    }
    else if (command == 0xdfa7c4ad)
    {

        (*response_size) = snprintf(response, resp_buf_size,
                                "Should return error code -128\n");
        return -128;
    }
    else
    {
        LOG_ERR("Unknown command %x", command);
        (*response_size) = 0;
    }

    return -PB_ERR;
}

int board_status(void *plat,
                    void *response_bfr,
                    size_t *response_size)
{
    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size,
                            "Board status OK!\n");
    return PB_OK;
}

int board_early_init(void *plat)
{
    int rc;

    rc = pb_storage_add(&virtio_driver);

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

int pb_qemu_console_init(struct qemu_uart_device *dev)
{
    dev->base = 0x09000000;
    return PB_OK;
}

const char *board_name(void)
{
    return "qemuarmv7";
}

static int board_early_boot(void *plat)
{
    LOG_DBG("call");
    return PB_OK;
}

static int board_late_boot(void *plat, uuid_t boot_part_uu, enum pb_boot_mode mode)
{
    LOG_DBG("Boot! (%i)", mode);
    char boot_part_uu_str[37];

    uuid_unparse(boot_part_uu, boot_part_uu_str);

    long fd = semihosting_file_open("/tmp/pb_boot_status", 6);

    if (fd < 0)
        return PB_ERR;

    size_t bytes_to_write = 1;
    char name = '?';

    if (strcmp(boot_part_uu_str, "2af755d8-8de5-45d5-a862-014cfa735ce0") == 0)
        name = 'A';
    else if (strcmp(boot_part_uu_str, "c046ccd8-0f2e-4036-984d-76c14dc73992") == 0)
        name = 'B';
    else
        name = '?';

    semihosting_file_write(fd, &bytes_to_write,
                            (const uintptr_t) &name);

    semihosting_file_close(fd);
    plat_reset();
    return -PB_ERR; /* Should not be reached */
}

const struct pb_boot_config * board_boot_config(void)
{
    static const struct pb_boot_config config = {
        .a_boot_part_uuid  = "2af755d8-8de5-45d5-a862-014cfa735ce0",
        .b_boot_part_uuid  = "c046ccd8-0f2e-4036-984d-76c14dc73992",
        .primary_state_part_uuid = "f5f8c9ae-efb5-4071-9ba9-d313b082281e",
        .backup_state_part_uuid  = "656ab3fc-5856-4a5e-a2ae-5a018313b3ee",
        .image_bpak_id     = 0xec103b08,    /* bpak_id("kernel") */
        .dtb_bpak_id       = 0,
        .ramdisk_bpak_id   = 0,
        .rollback_mode     = PB_ROLLBACK_MODE_NORMAL,
        .early_boot_cb     = board_early_boot,
        .late_boot_cb      = board_late_boot,
        .dtb_patch_cb      = NULL,
        .print_time_measurements = false,
    };

    return &config;
}
