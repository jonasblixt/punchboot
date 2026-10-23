/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * eMMC boot partition selection (src/drivers/mmc/mmc_core.c) on a fake card.
 *
 * The card keeps BOOT_PARTITION_ENABLE (EXT_CSD[179] bits 5:3) across power
 * cycles and clears PARTITION_ACCESS (bits 2:0). The i.MX6UL ROM loads the
 * partition in BOOT_PARTITION_ENABLE and has no fallback to the other boot
 * partition (RM 8.5.3.2), so an interrupted boot partition write must leave
 * the other one enabled until the next update. Command mode calls
 * bio_write_done() when the host finalizes a stream without errors.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cmocka.h>

#include <drivers/mmc/mmc_core.h>
#include <pb/bio.h>
#include <pb/delay.h>
#include <pb/pb.h>
#include <string.h>

#define PART_BLOCKS 256 /* BOOT_MULT and RPMB_MULT 1: 128 KiB, user area the same */
#define IMAGE_SIZE  (64 * 1024)
#define CHUNK_BYTES 4096

#define UU_BOOT0 \
    (const unsigned char *)"\x9e\xef\x75\x44\xbf\x68\x4b\xf7\x86\x78\xda\x11\x7c\xbc\xcb\xa8"
#define UU_BOOT1 \
    (const unsigned char *)"\x4e\xe3\x16\x90\x0c\x9b\x4d\x56\xa6\xa6\xe6\xd6\xec\xfd\x4d\x54"
#define UU_USER \
    (const unsigned char *)"\x1a\xad\x85\xa9\x75\xcd\x42\x6d\x8d\xc4\xe9\xbd\xfe\xeb\x68\x75"
#define UU_RPMB \
    (const unsigned char *)"\x8d\x75\xd8\xb9\xb1\x69\x4d\xe6\xbe\xe0\x48\xab\xdc\x95\xc4\x08"

/* The card, indexed by PARTITION_ACCESS: user, BOOT0, BOOT1, RPMB */
static struct {
    bool powered;
    uint8_t ext_csd[512];
    bool ext_csd_pending; /* CMD8 sent, the next read returns the EXT_CSD */
    uint8_t part[MMC_PART_END][PART_BLOCKS * 512];
    int blocks_before_power_loss; /* < 0: no power loss */
} card;

static uint8_t old_image[IMAGE_SIZE];
static uint8_t new_image[IMAGE_SIZE];

void pb_delay_ms(uint32_t delay)
{
    (void)delay;
}

static int fake_init(void)
{
    return PB_OK;
}

static int fake_send_cmd(uint16_t idx, uint32_t arg, uint16_t resp_type, mmc_cmd_resp_t result)
{
    (void)resp_type;

    if (!card.powered)
        return -PB_ERR_IO;

    if (result != NULL)
        memset(result, 0, sizeof(mmc_cmd_resp_t));

    switch (idx) {
    case MMC_CMD_SEND_OP_COND:
        result[0] = OCR_POWERUP | OCR_SECTOR_MODE;
        break;
    case MMC_CMD_SEND_STATUS:
        result[0] = (MMC_STATE_TRAN << 9) | STATUS_READY_FOR_DATA;
        break;
    case MMC_CMD_SWITCH:
        if ((arg & EXTCSD_WRITE_BYTES) == EXTCSD_WRITE_BYTES)
            card.ext_csd[(arg >> 16) & 0xff] = (arg >> 8) & 0xff;
        break;
    case MMC_CMD_SEND_EXT_CSD:
        card.ext_csd_pending = true;
        break;
    default:
        break;
    }

    return PB_OK;
}

static uint8_t *card_part(void)
{
    return card.part[card.ext_csd[EXT_CSD_PART_CONFIG] & 0x07];
}

static int fake_prepare(unsigned int lba, size_t length, uintptr_t buf)
{
    (void)lba;
    (void)length;
    (void)buf;
    return card.powered ? PB_OK : -PB_ERR_IO;
}

static int fake_read(unsigned int lba, size_t length, uintptr_t buf)
{
    if (card.ext_csd_pending) {
        card.ext_csd_pending = false;
        memcpy((void *)buf, card.ext_csd, length);
        return PB_OK;
    }

    memcpy((void *)buf, &card_part()[lba * 512], length);
    return PB_OK;
}

/* Writes block by block, and loses power after blocks_before_power_loss */
static int fake_write(unsigned int lba, size_t length, uintptr_t buf)
{
    for (size_t n = 0; n < length / 512; n++) {
        if (card.blocks_before_power_loss == 0) {
            card.powered = false;
            return -PB_ERR_IO;
        }
        if (card.blocks_before_power_loss > 0)
            card.blocks_before_power_loss--;

        memcpy(&card_part()[(lba + n) * 512], (const uint8_t *)buf + n * 512, 512);
    }

    return PB_OK;
}

static int fake_set_bus_clock(unsigned int clk_hz)
{
    (void)clk_hz;
    return PB_OK;
}

static int fake_set_bus_width(enum mmc_bus_width width)
{
    (void)width;
    return PB_OK;
}

static const struct mmc_hal hal = {
    .init = fake_init,
    .send_cmd = fake_send_cmd,
    .set_bus_clock = fake_set_bus_clock,
    .set_bus_width = fake_set_bus_width,
    .prepare = fake_prepare,
    .read = fake_read,
    .write = fake_write,
    .max_chunk_bytes = CHUNK_BYTES,
};

/* Same as jiffy */
static const struct mmc_device_config cfg = {
    .mode = MMC_BUS_MODE_DDR52,
    .width = MMC_BUS_WIDTH_8BIT_DDR,
    .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
    .boot0_uu = UU_BOOT0,
    .boot1_uu = UU_BOOT1,
    .user_uu = UU_USER,
    .rpmb_uu = UU_RPMB,
};

/* A provisioned card: the old image in both boot partitions, BOOT0 enabled */
static void card_provision(void)
{
    memset(&card, 0, sizeof(card));
    card.ext_csd[EXT_CSD_SEC_CNT] = PART_BLOCKS & 0xff;
    card.ext_csd[EXT_CSD_SEC_CNT + 1] = PART_BLOCKS >> 8;
    card.ext_csd[EXT_CSD_BOOT_MULT] = 1;
    card.ext_csd[EXT_CSD_RPMB_MULT] = 1;
    card.ext_csd[EXT_CSD_BOOT_BUS_CONDITIONS] = cfg.boot_mode;
    card.ext_csd[EXT_CSD_GENERIC_CMD6_TIME] = 1;
    card.ext_csd[EXT_CSD_PART_SWITCH_TIME] = 1;
    card.ext_csd[EXT_CSD_POWER_OFF_LONG_TIME] = 1;
    card.ext_csd[EXT_CSD_PART_CONFIG] = (MMC_PART_BOOT0 << 3) | MMC_PART_USER;
    card.blocks_before_power_loss = -1;

    memcpy(card.part[MMC_PART_BOOT0], old_image, IMAGE_SIZE);
    memcpy(card.part[MMC_PART_BOOT1], old_image, IMAGE_SIZE);
}

/*
 * Power on: the card clears PARTITION_ACCESS, and the ROM picks the partition
 * in BOOT_PARTITION_ENABLE. Returns the hardware partition the ROM loads.
 */
static enum mmc_part power_on(void)
{
    card.powered = true;
    card.ext_csd_pending = false;
    card.blocks_before_power_loss = -1;
    card.ext_csd[EXT_CSD_PART_CONFIG] &= ~0x07;

    switch ((card.ext_csd[EXT_CSD_PART_CONFIG] >> 3) & 0x07) {
    case 1:
        return MMC_PART_BOOT0;
    case 2:
        return MMC_PART_BOOT1;
    default:
        return MMC_PART_USER;
    }
}

static bool image_is(enum mmc_part part, const uint8_t *image)
{
    return memcmp(card.part[part], image, IMAGE_SIZE) == 0;
}

static int setup(void **state)
{
    (void)state;
    card_provision();
    return 0;
}

static bio_dev_t part(const unsigned char *uu)
{
    return bio_get_part_by_uu(uu);
}

/* What pb does on every boot: init the card, read the GPT from the user area */
static void pb_boot(void)
{
    uint8_t gpt_header[512];

    bio_reset();
    assert_int_equal(mmc_init(&hal, &cfg), PB_OK);
    assert_int_equal(bio_read(part(UU_USER), 1, sizeof(gpt_header), gpt_header), PB_OK);
}

/* A complete update of one boot partition, as `part write` does it */
static void update(const unsigned char *uu, const uint8_t *image)
{
    assert_int_equal(bio_write(part(uu), 0, IMAGE_SIZE, image), PB_OK);
    assert_int_equal(bio_write_done(part(uu)), PB_OK);
}

static void test_interrupted_boot0_write(void **state)
{
    (void)state;

    /* pb boots from BOOT0, and the host writes a new BOOT0 */
    assert_int_equal(power_on(), MMC_PART_BOOT0);
    pb_boot();

    card.blocks_before_power_loss = IMAGE_SIZE / 512 / 2;
    assert_int_not_equal(bio_write(part(UU_BOOT0), 0, IMAGE_SIZE, new_image), PB_OK);
    assert_false(card.powered);
    assert_false(image_is(MMC_PART_BOOT0, old_image));
    assert_false(image_is(MMC_PART_BOOT0, new_image));

    /* Every boot until the next update is from the intact BOOT1 */
    for (int n = 0; n < 3; n++) {
        print_message("Boot %i after the power loss\n", n + 1);
        assert_int_equal(power_on(), MMC_PART_BOOT1);
        assert_true(image_is(MMC_PART_BOOT1, old_image));
        pb_boot();
    }

    /* The next update completes and boots */
    update(UU_BOOT0, new_image);
    assert_int_equal(power_on(), MMC_PART_BOOT0);
    assert_true(image_is(MMC_PART_BOOT0, new_image));
}

static void test_update_both(void **state)
{
    (void)state;

    assert_int_equal(power_on(), MMC_PART_BOOT0);
    pb_boot();

    update(UU_BOOT0, new_image);
    update(UU_BOOT1, new_image);

    assert_int_equal(power_on(), MMC_PART_BOOT1);
    assert_true(image_is(MMC_PART_BOOT1, new_image));
    assert_true(image_is(MMC_PART_BOOT0, new_image));
}

/* Power is lost after the last block, before the host finalizes the stream */
static void test_write_not_finalized(void **state)
{
    (void)state;

    assert_int_equal(power_on(), MMC_PART_BOOT0);
    pb_boot();

    assert_int_equal(bio_write(part(UU_BOOT0), 0, IMAGE_SIZE, new_image), PB_OK);
    assert_int_equal(power_on(), MMC_PART_BOOT1);
}

/* The partition that isn't the boot partition is written in place */
static void test_update_other_part(void **state)
{
    (void)state;

    assert_int_equal(power_on(), MMC_PART_BOOT0);
    pb_boot();

    card.blocks_before_power_loss = IMAGE_SIZE / 512 / 2;
    assert_int_not_equal(bio_write(part(UU_BOOT1), 0, IMAGE_SIZE, new_image), PB_OK);
    assert_int_equal(power_on(), MMC_PART_BOOT0);
    pb_boot();

    update(UU_BOOT1, new_image);
    assert_int_equal(power_on(), MMC_PART_BOOT1);
}

/* Only a write changes the boot partition, a read or an RPMB access doesn't */
static void test_access_keeps_boot_part(void **state)
{
    uint8_t buf[512];
    (void)state;

    card.ext_csd[EXT_CSD_PART_CONFIG] |= BIT(6); /* BOOT_ACK */

    for (int boot = 0; boot < 2; boot++) {
        assert_int_equal(power_on(), boot ? MMC_PART_BOOT1 : MMC_PART_BOOT0);
        pb_boot();

        assert_int_equal(bio_read(part(UU_BOOT0), 0, sizeof(buf), buf), PB_OK);
        assert_int_equal(bio_read(part(UU_BOOT1), 0, sizeof(buf), buf), PB_OK);
        assert_int_equal(bio_read(part(UU_RPMB), 0, sizeof(buf), buf), PB_OK);
        assert_int_equal(power_on(), boot ? MMC_PART_BOOT1 : MMC_PART_BOOT0);
        assert_true(card.ext_csd[EXT_CSD_PART_CONFIG] & BIT(6));
        pb_boot();

        update(UU_BOOT1, new_image);
        assert_true(card.ext_csd[EXT_CSD_PART_CONFIG] & BIT(6));
    }
}

/* A write to the user area doesn't change the boot partition */
static void test_write_done_user(void **state)
{
    (void)state;

    assert_int_equal(power_on(), MMC_PART_BOOT0);
    pb_boot();

    assert_int_equal(bio_write(part(UU_USER), 0, 512, new_image), PB_OK);
    assert_int_equal(bio_write_done(part(UU_USER)), PB_OK);
    assert_int_equal(power_on(), MMC_PART_BOOT0);
}

/* Runs before any mmc_init() */
static void test_boot_part_config_before_init(void **state)
{
    uuid_t uu;
    (void)state;

    assert_int_equal(mmc_get_boot_part_config(uu), -PB_ERR_STATE);
}

/* The partition pb was loaded from, recorded by the first mmc_init() */
static void test_boot_part_config(void **state)
{
    uuid_t uu;
    (void)state;

    assert_int_equal(power_on(), MMC_PART_BOOT0);
    pb_boot();
    assert_int_equal(mmc_get_boot_part_config(uu), PB_OK);
    assert_memory_equal(uu, UU_BOOT0, sizeof(uu));

    /* The update enables BOOT1, but pb was still loaded from BOOT0 */
    update(UU_BOOT0, new_image);
    pb_boot();
    assert_int_equal(mmc_get_boot_part_config(uu), PB_OK);
    assert_memory_equal(uu, UU_BOOT0, sizeof(uu));
}

/*
 * The first mmc_init() of a boot records BOOT_PARTITION_ENABLE, so each one
 * runs in its own process, forked before this process calls mmc_init().
 * Returns mmc_get_boot_part_config() and its UUID.
 */
static int first_boot_part_config(uint8_t boot_part_enable, uuid_t uu)
{
    int fds[2];
    int status;

    struct {
        int rc;
        uuid_t uu;
    } result = { 0 };

    assert_int_equal(pipe(fds), 0);
    pid_t pid = fork();
    assert_true(pid >= 0);

    if (pid == 0) {
        /* exit(), not _exit(): gcov writes the coverage at exit */
        close(fds[0]);
        card.ext_csd[EXT_CSD_PART_CONFIG] = boot_part_enable << 3;
        power_on();
        bio_reset();
        if (mmc_init(&hal, &cfg) != PB_OK)
            exit(1);
        result.rc = mmc_get_boot_part_config(result.uu);
        exit(write(fds[1], &result, sizeof(result)) == sizeof(result) ? 0 : 1);
    }

    close(fds[1]);
    assert_int_equal(read(fds[0], &result, sizeof(result)), sizeof(result));
    close(fds[0]);
    assert_int_equal(waitpid(pid, &status, 0), pid);
    assert_true(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    memcpy(uu, result.uu, sizeof(uuid_t));
    return result.rc;
}

/* Runs before any mmc_init() */
static void test_boot_part_config_values(void **state)
{
    uuid_t uu;
    (void)state;

    assert_int_equal(first_boot_part_config(2, uu), PB_OK);
    assert_memory_equal(uu, UU_BOOT1, sizeof(uu));
    assert_int_equal(first_boot_part_config(0, uu), PB_OK);
    assert_memory_equal(uu, UU_USER, sizeof(uu));
    assert_int_equal(first_boot_part_config(7, uu), PB_OK);
    assert_memory_equal(uu, UU_USER, sizeof(uu));
    assert_int_equal(first_boot_part_config(3, uu), -PB_ERR_STATE); /* Reserved */
}

int main(void)
{
    for (size_t n = 0; n < IMAGE_SIZE; n++) {
        old_image[n] = (uint8_t)n;
        new_image[n] = (uint8_t)~n;
    }

    const struct CMUnitTest tests[] = {
        /* Before any mmc_init() */
        cmocka_unit_test(test_boot_part_config_before_init),
        cmocka_unit_test_setup(test_boot_part_config_values, setup),
        cmocka_unit_test_setup(test_interrupted_boot0_write, setup),
        cmocka_unit_test_setup(test_update_both, setup),
        cmocka_unit_test_setup(test_write_not_finalized, setup),
        cmocka_unit_test_setup(test_update_other_part, setup),
        cmocka_unit_test_setup(test_access_keeps_boot_part, setup),
        cmocka_unit_test_setup(test_write_done_user, setup),
        cmocka_unit_test_setup(test_boot_part_config, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
