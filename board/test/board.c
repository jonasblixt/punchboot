/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <board.h>
#include <fuse.h>
#include <plat.h>
#include <gpt.h>
#include <plat/test/plat.h>
#include <plat/test/semihosting.h>

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

const struct partition_table pb_partition_table[] =
{
    PB_GPT_ENTRY(1024, PB_PARTUUID_SYSTEM_A, "System A"),
    PB_GPT_ENTRY(1024, PB_PARTUUID_SYSTEM_B, "System B"),
    PB_GPT_ENTRY(1, PB_PARTUUID_CONFIG_PRIMARY, "Config Primary"),
    PB_GPT_ENTRY(1, PB_PARTUUID_CONFIG_BACKUP, "Config Backup"),
    PB_GPT_END,
};

/* Punchboot board API */

uint32_t board_early_init(struct pb_platform_setup *plat)
{
    plat->uart_base = 0x09030000;

    return PB_OK;
}

uint32_t board_late_init(struct pb_platform_setup *plat)
{
    UNUSED(plat);


    long fd = semihosting_file_open("/tmp/pb_boot_status", 6);

    if (fd < 0)
        return PB_ERR;

    const char boot_status[] = "NONE";

    size_t bytes_to_write = strlen(boot_status);

    semihosting_file_write(fd, &bytes_to_write,
                            (const uintptr_t) boot_status);

    semihosting_file_close(fd);
    return PB_OK;
}

uint32_t board_prepare_recovery(struct pb_platform_setup *plat)
{
    UNUSED(plat);
    return PB_OK;
}

bool board_force_recovery(struct pb_platform_setup *plat)
{
    UNUSED(plat);

    long fd = semihosting_file_open("/tmp/pb_force_recovery", 0);

    if (fd < 0)
        return false;

    semihosting_file_close(fd);

    return true;
}

uint32_t board_setup_device(struct param *params)
{
    uint32_t err;
    uint32_t v;
    struct param *p;

    err = param_get_by_id(params, "device_id", &p);

    if (err != PB_OK)
        return err;

    err = param_get_u32(p, &v);

    if (err != PB_OK)
        return err;

    LOG_INFO("Device ID: 0x%08x", v);

    board_ident_fuse.value = v;

    return plat_fuse_write(&board_ident_fuse);
}

uint32_t board_setup_lock(void)
{
    return PB_OK;
}

uint32_t board_get_params(struct param **pp)
{
    param_add_str((*pp)++, "Board", "Test");

    return PB_OK;
}

uint32_t board_recovery_command(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                uint32_t arg3)
{
    UNUSED(arg0);
    UNUSED(arg1);
    UNUSED(arg2);
    UNUSED(arg3);

    return PB_ERR;
}
