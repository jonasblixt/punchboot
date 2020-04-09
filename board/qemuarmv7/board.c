/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
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
#include <pb/crypto.h>
#include <pb/boot.h>
#include <pb/boot_ab.h>
#include <plat/qemu/plat.h>
#include <plat/qemu/uart.h>
#include <plat/qemu/semihosting.h>
#include <plat/qemu/virtio_block.h>
#include <plat/qemu/virtio_serial.h>
#include <plat/qemu/transport.h>
#include <bearssl/bearssl.h>
#include <libfdt.h>

const struct fuse fuses[] =
{
    TEST_FUSE_BANK_WORD_VAL(5,  "SRK0",  0x5020C7D7),
    TEST_FUSE_BANK_WORD_VAL(6,  "SRK1",  0xBB62B945),
    TEST_FUSE_BANK_WORD_VAL(7,  "SRK2",  0xDD97C8BE),
    TEST_FUSE_BANK_WORD_VAL(8,  "SRK3",  0xDC6710DD),
    TEST_FUSE_BANK_WORD_VAL(9,  "SRK4",  0x2756B777),
    TEST_FUSE_BANK_WORD_VAL(10, "SRK5",  0xEF43BC0A),
    TEST_FUSE_BANK_WORD_VAL(11, "SRK6",  0x7185604B),
    TEST_FUSE_BANK_WORD_VAL(12, "SRK7",  0x3F335991),
    TEST_FUSE_BANK_WORD_VAL(13, "BOOT0", 0x12341234),
    TEST_FUSE_BANK_WORD_VAL(14, "BOOT1", 0xCAFEEFEE),
    TEST_FUSE_END,
};

static struct fuse board_ident_fuse =
        TEST_FUSE_BANK_WORD(15, "Ident");

#define DEF_FLAGS (PB_STORAGE_MAP_FLAG_WRITABLE | \
                   PB_STORAGE_MAP_FLAG_VISIBLE)

const struct pb_storage_map map[] =
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

    PB_STORAGE_MAP_END
};

/* CONSOLE0 driver */

static struct qemu_uart_device con0_device =
{
    .base = 0x09000000,
};

static struct pb_console_driver con0_driver =
{
    .name = "con0",
    .platform = NULL,
    .private = &con0_device,
    .size = sizeof(con0_device),
};

/* CRYPTO0 driver */

uint8_t crypto0_private[4096] __a4k __no_bss;

static struct pb_crypto_driver crypto0_driver =
{
    .platform = NULL,
    .private = crypto0_private,
    .size = sizeof(crypto0_private),
};

/* END of CRYPTO0*/

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

static struct pb_storage_map_driver virtio_map_driver =
{
    .map_data = map_data,
    .map_size = sizeof(map_data),
    .private = gpt_private_data,
    .size = sizeof(gpt_private_data),
};

static struct pb_storage_driver virtio_driver =
{
    .name = "virtio0",
    .block_size = 512,
    .default_map = map,
    .map = &virtio_map_driver,
    .platform = NULL,
    .private = &virtio_block,
    .last_block = 65535,
};

/* END of storage */

/* Transport configuration */

static struct virtio_serial_device virtio_serial0 __a4k =
{
    .dev =
    {
        .device_id = 3,
        .vendor_id = 0x554D4551,
        .base = 0x0A003E00,
    },
};

static struct pb_transport_driver virtio_serial0_driver =
{
    .name = "virtio_serial0",
    .platform = NULL,
    .private = &virtio_serial0,
    .size = sizeof(virtio_serial0),
};

/* Command mode buffers */
#define PB_CMD_BUFFER_SIZE (1024*1024*4)
#define PB_CMD_NO_OF_BUFFERS 2

static uint8_t command_buffers[PB_CMD_NO_OF_BUFFERS][PB_CMD_BUFFER_SIZE] __no_bss __a4k;

/* Boot driver */

static uint8_t boot_state[512] __no_bss __a4k;
static uint8_t boot_state_backup[512] __no_bss __a4k;
static struct pb_image_load_context load_ctx __no_bss __a4k;

static int jump(struct pb_boot_driver *boot)
{
    struct pb_boot_ab_driver *bdrv = boot->private;
    //arch_jump((void *) boot->jump_addr, NULL, NULL, NULL, NULL);
    LOG_DBG("Boot!");

    long fd = semihosting_file_open("/tmp/pb_boot_status", 6);

    if (fd < 0)
        return PB_ERR;

    size_t bytes_to_write = strlen(bdrv->active->name);

    semihosting_file_write(fd, &bytes_to_write,
                            (const uintptr_t) bdrv->active->name);

    semihosting_file_close(fd);
    plat_reset();
    return -PB_ERR; /* Should not be reached */
}

static struct pb_boot_ab_driver ab_boot_driver =
{
    .a =
    {
        .name = "A",
        .image = "2af755d8-8de5-45d5-a862-014cfa735ce0",
        .board_private = NULL,
    },
    .b =
    {
        .name = "B",
        .image = "c046ccd8-0f2e-4036-984d-76c14dc73992",
        .board_private = NULL,
    },
};

/* Board specific */

static int board_pre_boot(struct pb_board *board)
{
    long fd = semihosting_file_open("/tmp/pb_force_command_mode", 0);

    LOG_DBG("Checking for /tmp/pb_force_command_mode");

    if (fd != -1)
    {
        board->force_command_mode = true;
        semihosting_file_close(fd);
    }

    return PB_OK;
}


static struct pb_boot_driver boot_driver =
{
    .on_dt_patch_bootargs = NULL,
    .on_jump = jump,
    .boot_image_id = 0xec103b08,    /* atf     */
    .ramdisk_image_id = 0, /* ramdisk */
    .dtb_image_id = 0,     /* dt */
    .state = (void *) boot_state,
    .backup_state = (void *) boot_state_backup,
    .primary_state_uu = "f5f8c9ae-efb5-4071-9ba9-d313b082281e",
    .backup_state_uu  = "656ab3fc-5856-4a5e-a2ae-5a018313b3ee",
    .load_ctx = &load_ctx,
    .private = &ab_boot_driver,
    .size = sizeof(ab_boot_driver),
};

int board_early_init(struct pb_platform_setup *plat,
                          struct pb_storage *storage,
                          struct pb_transport *transport,
                          struct pb_console *console,
                          struct pb_crypto *crypto,
                          struct pb_command_context *command_ctx,
                          struct pb_boot_context *boot,
                          struct bpak_keystore *keystore,
                          struct pb_board *board)
{
    int rc;

    board->name = "qemuarmv7";
    board->force_command_mode = false;
    board->pre_boot = board_pre_boot;

    command_ctx->buffer = command_buffers;
    command_ctx->no_of_buffers = PB_CMD_NO_OF_BUFFERS;
    command_ctx->buffer_size = PB_CMD_BUFFER_SIZE;

    /* Configure console */

    rc = qemu_uart_setup(&con0_driver);

    if (rc != PB_OK)
        return rc;

    rc = pb_console_add(console, &con0_driver);

    if (rc != PB_OK)
        return rc;

    /* Configure crypto */

    rc = bearssl_setup(&crypto0_driver);

    if (rc != PB_OK)
        return rc;

    rc = pb_crypto_add(crypto, &crypto0_driver);

    if (rc != PB_OK)
        return rc;

    /* Configure storage */

    rc = virtio_block_setup(&virtio_driver);

    if (rc != PB_OK)
        return rc;

    rc = pb_gpt_map_init(&virtio_driver);

    if (rc != PB_OK)
        return rc;

    rc = pb_storage_add(storage, &virtio_driver);

    if (rc != PB_OK)
        return rc;

    /* Configure command transport */

    rc = virtio_serial_transport_setup(transport, &virtio_serial0_driver);
   
    if (rc != PB_OK)
        return rc;

    rc = pb_transport_add(transport, &virtio_serial0_driver);

    if (rc != PB_OK)
        return rc;

    /* Configure boot driver */

    rc = pb_boot_ab_init(boot, &boot_driver, storage, crypto, keystore);

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

int board_late_init(struct pb_platform_setup *plat)
{
    UNUSED(plat);


    return PB_OK;
}


