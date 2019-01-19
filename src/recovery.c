/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <recovery.h>
#include <image.h>
#include <plat.h>
#include <usb.h>
#include <keys.h>
#include <io.h>
#include <board.h>
#include <tinyprintf.h>
#include <gpt.h>
#include <config.h>
#include <string.h>
#include <boot.h>
#include <uuid.h>
#include <board/config.h>
#include <plat/defs.h>

#define RECOVERY_CMD_BUFFER_SZ  1024*64
#define RECOVERY_BULK_BUFFER_SZ 1024*1024*8

static uint8_t __a4k __no_bss recovery_cmd_buffer[RECOVERY_CMD_BUFFER_SZ];
static uint8_t __a4k __no_bss recovery_bulk_buffer[2][RECOVERY_BULK_BUFFER_SZ];
static char __no_bss report_text_buffer[1024*10];

extern const struct fuse uuid_fuses[];
extern const struct fuse device_info_fuses[];
extern const struct fuse root_hash_fuses[];
extern const struct fuse board_fuses[];
extern const uint32_t build_root_hash[];

const char *recovery_cmd_name[] =
{
    "PB_CMD_RESET",
    "PB_CMD_FLASH_BOOTLOADER",
    "PB_CMD_PREP_BULK_BUFFER",
    "PB_CMD_GET_VERSION",
    "PB_CMD_GET_GPT_TBL",
    "PB_CMD_WRITE_PART",
    "PB_CMD_BOOT_PART",
    "PB_CMD_GET_CONFIG_TBL",
    "PB_CMD_GET_CONFIG_VAL",
    "PB_CMD_SET_CONFIG_VAL",
    "PB_CMD_READ_UUID",
    "PB_CMD_WRITE_DFLT_GPT",
    "PB_CMD_BOOT_RAM",
    "PB_CMD_SETUP",
    "PB_CMD_SETUP_LOCK",
};


static uint32_t recovery_flash_bootloader(uint8_t *bfr, 
                                          uint32_t blocks_to_write) 
{
    if (plat_switch_part(PLAT_EMMC_PART_BOOT0) != PB_OK) 
    {
        LOG_ERR ("Could not switch partition");
        return PB_ERR;
    }

    plat_switch_part(PLAT_EMMC_PART_BOOT0);
    plat_write_block(PB_BOOTPART_OFFSET, bfr, blocks_to_write);

    plat_switch_part(PLAT_EMMC_PART_BOOT1);
    plat_write_block(PB_BOOTPART_OFFSET, bfr, blocks_to_write);

    plat_switch_part(PLAT_EMMC_PART_USER);
 
    return PB_OK;
}

static uint32_t recovery_flash_part(uint8_t part_no, 
                                    uint32_t lba_offset, 
                                    uint32_t no_of_blocks, 
                                    uint8_t *bfr) 
{
    uint32_t part_lba_offset = 0;

    part_lba_offset = gpt_get_part_first_lba(part_no);

    if (!part_lba_offset) 
    {
        LOG_ERR ("Unknown partition");
        return PB_ERR;
    }

    if ( (lba_offset + no_of_blocks) > gpt_get_part_last_lba(part_no))
    {
        LOG_ERR ("Trying to write outside of partition");
        return PB_ERR;
    }
    
    return plat_write_block(part_lba_offset + lba_offset, 
                                bfr, no_of_blocks);
}

static uint32_t recovery_send_response(struct usb_device *dev, 
                                       uint8_t *bfr, uint32_t sz)
{
    uint32_t err = PB_OK;
    
    memcpy(recovery_cmd_buffer, (uint8_t *) &sz, 4);

    if (sz >= RECOVERY_CMD_BUFFER_SZ)
        return PB_ERR;

    err = plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, 4);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(dev, USB_EP3_IN);

    if (sz > RECOVERY_CMD_BUFFER_SZ)
        return PB_ERR;

    memcpy(recovery_cmd_buffer, bfr, sz);
 
    err = plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, sz);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(dev, USB_EP3_IN);

    return PB_OK;
}

static uint32_t recovery_read_data(struct usb_device *dev,
                                   uint8_t *bfr, uint32_t sz)
{
    uint32_t err = PB_OK;

    err = plat_usb_transfer(dev, USB_EP2_OUT, recovery_cmd_buffer, sz);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(dev, USB_EP2_OUT);

    memcpy(bfr, recovery_cmd_buffer, sz);

    return err;
}

static void recovery_send_result_code(struct usb_device *dev, uint32_t value)
{
    /* Send result code */
    memcpy(recovery_cmd_buffer, (uint8_t *) &value, sizeof(uint32_t));
    plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, sizeof(uint32_t));
    plat_usb_wait_for_ep_completion(dev, USB_EP3_IN);
}


static uint32_t recovery_setup_device(struct usb_device *dev,
                                      struct pb_device_setup *pb_setup)
{
    uint32_t pos = 0;
    uint32_t err;
    uint32_t n;

    uint8_t device_uuid[16];
    char device_uuid_str[37];
    bool flag_uuid_fused = false;

    uint32_t root_hash[8];
    bool flag_root_hash_fused = false;

    bool flag_devid_fused = false;
    bool flag_devid_revvar_fused = false;

    bool flag_board_fused = true;
    /* UUID */

    n = 0;
    foreach_fuse_read(f, uuid_fuses)
    {
        memcpy(&device_uuid[n], (uint32_t *)&f->value, 4);
        n += 4;
    }

    for (uint32_t n = 0; n < 16; n++)
    {
        if (device_uuid[n] != 0)
            flag_uuid_fused = true;
    }

    if (flag_uuid_fused)
    {
        uuid_to_string(device_uuid, device_uuid_str);
    } else {
        uuid_to_string(pb_setup->uuid, device_uuid_str);
        /* Prepare UUID fuses to be written */
        n = 0;
        foreach_fuse(f, uuid_fuses)
        {
            memcpy((uint32_t *) &f->value, &(pb_setup->uuid[n]), 4);
            n += 4;
        }
    }

    /* Root hash */
    n = 0;
    foreach_fuse_read(f, root_hash_fuses)
    {
        root_hash[n++] = f->value;
        if (f->value != 0)
            flag_root_hash_fused = true;
    }

    if (!flag_root_hash_fused)
    {
        uint32_t *root_hash_part = build_root_hash;

        foreach_fuse(f, root_hash_fuses)
        {
            f->value = *root_hash_part++;
        }
    }

    /* Device identity */
    struct fuse *devid = (struct fuse *) device_info_fuses;
    err = plat_fuse_read(devid);
    
    if (err != PB_OK)
    {
        LOG_ERR("Could not access device identity fuse");
        return err;
    }  

    if ((devid->value & 0xFFFF0000) != 0)
    {
        flag_devid_fused = true;
    } else {
        devid->value = devid->default_value;
    }

    if ((devid->value & 0x0000FFFF) != 0)
    {
        flag_devid_revvar_fused = true;
    } else {
        devid->value |= ((pb_setup->device_variant << 8) & 0xff00) |
                        ((pb_setup->device_revision) & 0xff);
    }

    /* Board fuses */

    flag_board_fused = true;

    foreach_fuse_read(f, board_fuses)
    {
        if ((f->value & f->default_value) != f->default_value)
            flag_board_fused = false;
    }
        
    if (!flag_board_fused)
    {
        foreach_fuse(f, board_fuses)
            f->value = f->default_value;
    }  

    if(pb_setup->dry_run)
    {
        pos += tfp_sprintf(&report_text_buffer[pos], "-- Device setup report --\n");

        /* UUID */
        if (!flag_uuid_fused)
        {
            pos += tfp_sprintf(&report_text_buffer[pos], 
                    "Will Write UUID (%s) to:\n",
                    device_uuid_str);
 

        } else {
            pos += tfp_sprintf(&report_text_buffer[pos],
                        "UUID already fused (%s)\n", device_uuid_str);
        }
        
        foreach_fuse(f, uuid_fuses)
            pos += plat_fuse_to_string(f, &report_text_buffer[pos], 64);

        /* Root hash */
        if (!flag_root_hash_fused)
        {
            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Will write root key hash:\n");
        } else {
            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Root key hash already fused:\n");
        }

        foreach_fuse(f, root_hash_fuses)
            pos += plat_fuse_to_string(f, &report_text_buffer[pos], 64);

        /* Device identity */

        if (!flag_devid_fused)
        {
            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Will write device identity: %4.4lX\n", 
                            devid->default_value >> 16);
        } else {

            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Device identity already fused: %4.4lX\n", 
                            devid->value >> 16);

            if ( (devid->value & 0xFFFF0000) != devid->default_value)
            {
                pos += tfp_sprintf(&report_text_buffer[pos],
                            "  WARNING: device id mismatch with build in value\n");

                pos += tfp_sprintf(&report_text_buffer[pos],
                            "    0x%8.8lX != 0x%8.8lX\n", 
                                (devid->value & 0xFFFF0000), 
                                devid->default_value);
            }
        }

        if (!flag_devid_revvar_fused)
        {
            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Will write device variant: %2.2X, rev: %2.2X\n", 
                            (uint8_t) pb_setup->device_variant,
                            (uint8_t) pb_setup->device_revision);
        } else {

            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Device var/rev already fused, var: %2.2X, rev: %2.2X \n", 
                            (uint8_t) ((devid->value >> 8) & 0xff),
                            (uint8_t) (devid->value & 0xff));
        }

        pos += plat_fuse_to_string(devid, &report_text_buffer[pos], 64);

        /* Board fuses */
        if (!flag_board_fused)
        {
            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Will write board fuses\n");
        } else {

            pos += tfp_sprintf(&report_text_buffer[pos],
                        "Board fuses already fused\n");
        }

        foreach_fuse(f, board_fuses)
            pos += plat_fuse_to_string(f, &report_text_buffer[pos], 64);

        report_text_buffer[pos] = 0;
        recovery_send_response(dev, (uint8_t *) report_text_buffer, pos+1);
    } else {
        /* Perform the actual fuse programming */

        if (!flag_uuid_fused)
        {
            foreach_fuse(f, uuid_fuses)
            {
                err = plat_fuse_write(f);

                if (err != PB_OK)
                    return err;
            }
        }

        if (!flag_root_hash_fused)
        {
            foreach_fuse(f, root_hash_fuses)
            {
                err = plat_fuse_write(f);
                if (err != PB_OK)
                    return err;
            }
        }

        if (!flag_devid_fused || !flag_devid_revvar_fused)
        {
            err = plat_fuse_write(devid);

                if (err != PB_OK)
                    return err;
        }

        if (!flag_board_fused)
        {
            foreach_fuse(f, board_fuses)
            {
                err = plat_fuse_write(f);

                if (err != PB_OK)
                    return err;
            }
        }
    }


    return PB_OK;
}

static void recovery_parse_command(struct usb_device *dev, 
                                       struct usb_pb_command *cmd)
{
    uint32_t err = PB_OK;
    extern char end;

    LOG_INFO ("0x%8.8lX %s, sz=%lub", cmd->command, 
                                      recovery_cmd_name[cmd->command],
                                      cmd->size);

    switch (cmd->command) 
    {
        case PB_CMD_PREP_BULK_BUFFER:
        {
            struct pb_cmd_prep_buffer cmd_prep;

            recovery_read_data(dev, (uint8_t *) &cmd_prep,
                                sizeof(struct pb_cmd_prep_buffer));
 
            LOG_INFO("Preparing buffer %lu [%lu]",
                            cmd_prep.buffer_id, cmd_prep.no_of_blocks);
            
            if ( (cmd_prep.no_of_blocks*512) >= RECOVERY_BULK_BUFFER_SZ)
            {
                err = PB_ERR;
                break;
            }
            if (cmd_prep.buffer_id > 1)
            {
                err = PB_ERR;
                break;
            }

            uint8_t *bfr = recovery_bulk_buffer[cmd_prep.buffer_id];

            err = plat_usb_transfer(dev, USB_EP1_OUT, bfr,
                                                cmd_prep.no_of_blocks*512);

        }
        break;
        case PB_CMD_FLASH_BOOTLOADER:
        {
            uint32_t no_of_blks = 0;
            recovery_read_data(dev, (uint8_t *) &no_of_blks, 4);

            LOG_INFO ("Flash BL %li",no_of_blks);
            recovery_flash_bootloader(recovery_bulk_buffer[0], no_of_blks);
        }
        break;
        case PB_CMD_GET_VERSION:
        {
            char version_string[255];

            LOG_INFO ("Get version");
            tfp_sprintf(version_string, "PB %s",VERSION);

            err = recovery_send_response( dev, 
                                          (uint8_t *) version_string,
                                          strlen(version_string));
        }
        break;
        case PB_CMD_RESET:
        {
            err = PB_OK;
            recovery_send_result_code(dev, err);
            plat_reset();
            while(1)
                asm("wfi");
        }
        break;
        case PB_CMD_GET_GPT_TBL:
        {
            uint8_t *bfr = (uint8_t *) gpt_get_tbl();

            recovery_send_response(dev, bfr, sizeof (struct gpt_primary_tbl));          
        }
        break;
        case PB_CMD_GET_CONFIG_TBL:
        {
            LOG_INFO ("Read config %li", config_get_tbl_sz());

            err = recovery_send_response(dev, config_get_tbl(),
                                              config_get_tbl_sz());
        }
        break;
        case PB_CMD_SET_CONFIG_VAL:
        {
            uint32_t data[2];
            int32_t tmp_val;
            struct pb_config_item *items = config_get_tbl();

            recovery_read_data(dev, (uint8_t *) data, 8);
            

            err = config_get_uint32_t(data[0], &tmp_val);

            if (err != PB_OK)
                break;

            if (items[data[0]].access != PB_CONFIG_ITEM_RW)
            {
                err = PB_ERR;
                LOG_ERR ("Key is read only");
                break;
            }

            LOG_INFO("Set key %lu to %lu", data[0], data[1]);
            err = config_set_uint32_t(data[0], data[1]);
            if (err != PB_OK)
                break;
            err = config_commit();
        }
        break;
        case PB_CMD_GET_CONFIG_VAL:
        {
            uint32_t config_param = 0;
            uint32_t config_val;

            LOG_INFO("Reading key index, sz=%lu",cmd->size);
            recovery_read_data(dev, (uint8_t *) &config_param, 4);

            err = config_get_uint32_t(config_param, &config_val);

            if (err != PB_OK)
                config_val = 0;

            recovery_send_response(dev, (uint8_t *) &config_val, 4);
        }
        break;
        case PB_CMD_WRITE_PART:
        {
            struct pb_cmd_write_part wr_part;
        
            recovery_read_data(dev, (uint8_t *) &wr_part,
                                    sizeof(struct pb_cmd_write_part));
            
            LOG_INFO ("Writing %li blks to part %li with offset %8.8lX using bfr %li",
                        wr_part.no_of_blocks, wr_part.part_no,
                        wr_part.lba_offset, wr_part.buffer_id);

            err = recovery_flash_part(wr_part.part_no, 
                                wr_part.lba_offset, 
                                wr_part.no_of_blocks, 
                                recovery_bulk_buffer[wr_part.buffer_id]);
        }
        break;
        case PB_CMD_BOOT_PART:
        {
            uint8_t boot_part = 0;
            struct pb_pbi *pbi = NULL;

            err = recovery_read_data(dev, &boot_part, sizeof(uint8_t));

            if (err != PB_OK)
                break;
        
            err = pb_boot_load_part((uint8_t) boot_part, &pbi);
            
            if (err != PB_OK)
                break;

            recovery_send_result_code(dev, err);
            pb_boot_image(pbi, boot_part);

        }

        break;
        case PB_CMD_BOOT_RAM:
        {   
            struct pb_pbi *pbi = pb_image();

            recovery_read_data(dev, (uint8_t *) pbi, sizeof(struct pb_pbi));

            if (pbi->hdr.header_magic != PB_IMAGE_HEADER_MAGIC) 
            {
                LOG_ERR ("Incorrect header magic");
                err = PB_ERR;
            }

            if (err != PB_OK)
                break;

            LOG_INFO ("Component manifest:");

            for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
            {
                LOG_INFO (" o %lu - LA: 0x%8.8lX OFF:0x%8.8lX",i, 
                                    pbi->comp[i].load_addr_low,
                                    pbi->comp[i].component_offset);

                if (pbi->comp[i].load_addr_low < ((unsigned int) &end))
                {
                    LOG_ERR("image overlapping with bootloader memory");
                    err = PB_ERR;
                    break;
                }
            }


            if (err != PB_OK)
                break;

            recovery_send_result_code(dev, err);

            for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
            {
                LOG_INFO("Loading component %lu, %lu bytes",i, 
                                        pbi->comp[i].component_size);



                err = plat_usb_transfer(dev, USB_EP1_OUT,
                                        (uint8_t *) pbi->comp[i].load_addr_low,
                                        pbi->comp[i].component_size );

                plat_usb_wait_for_ep_completion(dev, USB_EP1_OUT);

                if (err != PB_OK)
                {
                    LOG_ERR ("Xfer error");
                    break;
                }
            }
    
            LOG_INFO("Loading done");

            if (err != PB_OK)
                break;

            if (pb_image_verify(pbi) == PB_OK)
            {
                LOG_INFO("Booting image...");
                recovery_send_result_code(dev, err);
                PB_BOOT_FUNC(pbi, SYSTEM_A);
            } else {
                LOG_ERR("Image verification failed");
                err = PB_ERR;
            }

        }
        break;
        case PB_CMD_READ_UUID:
        {
            uint8_t board_uuid[16];
            uint32_t n = 0;

            foreach_fuse(f, uuid_fuses)
            {
                memcpy(&board_uuid[n], (uint32_t *) &f->value, 4);
                n += 4;
            }

            recovery_send_response(dev, board_uuid, 16);
        }
        break;
        case PB_CMD_WRITE_DFLT_GPT:
        {
            LOG_INFO ("Installing default GPT table");

            err = gpt_init_tbl(1, plat_get_lastlba());

            if (err != PB_OK)
                break;

            err = gpt_add_part(0, 1, part_type_config, "Config");

            if (err != PB_OK)
                break;

            err = board_configure_gpt_tbl();

            if (err != PB_OK)
                break;

            err = gpt_write_tbl();
        }
        break;
        case PB_CMD_SETUP:
        {
            struct pb_device_setup pb_setup;
            LOG_INFO ("Performing device setup");

            recovery_read_data(dev, (uint8_t *) &pb_setup,
                                sizeof(struct pb_device_setup));
            
            err = recovery_setup_device(dev, &pb_setup);

        }
        break;
        case PB_CMD_SETUP_LOCK:
        {
            LOG_INFO ("Locking device setup");
            /* TODO: Implement */
        }
        break;
        default:
            LOG_ERR ("Got unknown command: %lx",cmd->command);
    }
    

    recovery_send_result_code(dev, err);

}

void recovery_initialize(void)
{
    usb_set_on_command_handler(recovery_parse_command);
}

