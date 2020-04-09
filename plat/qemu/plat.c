
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
#include <pb/io.h>
#include <pb/plat.h>
#include <pb/crypto.h>
#include <uuid/uuid.h>
#include <pb/fuse.h>
#include <pb/board.h>
#include <pb/boot.h>
#include <plat/qemu/virtio.h>
#include <plat/qemu/virtio_block.h>
#include <plat/qemu/virtio_serial.h>
#include <plat/qemu/test_fuse.h>
#include <plat/qemu/gcov.h>
#include <bearssl/bearssl_hash.h>
#include <plat/qemu/semihosting.h>
#include <bpak/bpak.h>

static struct pb_platform_setup setup;
static uint32_t setup_locked = 0;
extern const struct fuse fuses[];

__inline uint32_t plat_get_ms_tick(void)
{
    return 1;
}


int plat_get_security_state(uint32_t *state)
{
#ifdef __NOPE
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
#endif
    return PB_OK;
}

static const char *platform_namespace_uuid =
    "\x3f\xaf\xc6\xd3\xc3\x42\x4e\xdf\xa5\xa6\x0e\xb1\x39\xa7\x83\xb5";

static const char *device_unique_id =
    "\xbe\x4e\xfc\xb4\x32\x58\xcd\x63";

int plat_get_uuid(struct pb_crypto *crypto, char *out)
{
    int rc;
    char device_uu_str[37];

    rc = uuid_gen_uuid3(crypto, platform_namespace_uuid,
                          (const char *) device_unique_id, 8, out);

    uuid_unparse((const unsigned char *) out, device_uu_str);
    LOG_DBG("Get UUID %s", device_uu_str);

    return rc;
}

int plat_setup_device(void)
{
#ifdef __NOPE
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
#endif
    return PB_OK;
}


int plat_setup_lock(void)
{
    if (setup_locked)
        return PB_ERR;
    setup_locked = 1;
    return PB_OK;
}

int plat_fuse_read(struct fuse *f)
{
    uint32_t tmp_val = 0;
    int err;

    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return -PB_ERR;

    err = test_fuse_read(f->bank, &tmp_val);

    if (err != PB_OK)
        LOG_ERR("Could not read fuse");

    f->value = tmp_val;

    return err;
}

int plat_fuse_write(struct fuse *f)
{
#ifdef __NOPE
    int err;

    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return -PB_ERR;
    LOG_DBG("Writing fuse %s", f->description);

    err = test_fuse_write(&virtio_block, f->bank, f->value);

    if (err != PB_OK)
        LOG_ERR("Could not write fuse");
    return err;
#endif
    return PB_OK;
}

int plat_fuse_to_string(struct fuse *f, char *s, uint32_t n)
{
    if ( (f->status & FUSE_VALID) != FUSE_VALID)
        return -PB_ERR;

    return snprintf(s, n, "FUSE <%u> %s = 0x%08x\n", f->bank,
                f->description, f->value);
}

int plat_early_init(struct pb_storage *storage,
                    struct pb_transport *transport,
                    struct pb_console *console,
                    struct pb_crypto *crypto,
                    struct pb_command_context *command_ctx,
                    struct pb_boot_context *boot,
                    struct bpak_keystore *keystore,
                    struct pb_board *board)
{
    int rc;

    rc = board_early_init(&setup,
                    storage,
                    transport,
                    console,
                    crypto,
                    command_ctx,
                    boot,
                    keystore,
                    board);

    LOG_INFO("Plat start %i", rc);
    gcov_init();

    //test_fuse_init(&virtio_block);

    LOG_DBG("Done");
    return PB_OK;
}

void plat_preboot_cleanup(void)
{
}

uint32_t plat_get_us_tick(void)
{
    return 0;
}


