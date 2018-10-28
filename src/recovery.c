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
#include <recovery_protocol.h>

#define RECOVERY_CMD_BUFFER_SZ  1024*64
#define RECOVERY_BULK_BUFFER_SZ 1024*1024*8

static uint8_t __a4k __no_bss recovery_cmd_buffer[RECOVERY_CMD_BUFFER_SZ];
static uint8_t __a4k __no_bss recovery_bulk_buffer[2][RECOVERY_BULK_BUFFER_SZ];

static uint32_t recovery_flash_bootloader(uint8_t *bfr, 
                                          uint32_t blocks_to_write) 
{
    if (plat_emmc_switch_part(PLAT_EMMC_PART_BOOT0) != PB_OK) 
    {
        LOG_ERR ("Could not switch partition");
        return PB_ERR;
    }

    plat_emmc_switch_part(PLAT_EMMC_PART_BOOT0);
    plat_emmc_write_block(2, bfr, blocks_to_write);

    plat_emmc_switch_part(PLAT_EMMC_PART_BOOT1);
    plat_emmc_write_block(2, bfr, blocks_to_write);

    plat_emmc_switch_part(PLAT_EMMC_PART_USER);
 
    return PB_OK;
}

static uint32_t recovery_flash_part(uint8_t part_no, 
                                    uint32_t lba_offset, 
                                    uint32_t no_of_blocks, 
                                    uint8_t *bfr) 
{
    uint32_t part_lba_offset = 0;

    // Check for data complete

    part_lba_offset = gpt_get_part_offset(part_no);

    if (!part_lba_offset) 
    {
        LOG_ERR ("Unknown partition");
        return PB_ERR;
    }
    
    plat_emmc_write_block(part_lba_offset + lba_offset, bfr, no_of_blocks);

    return PB_OK;
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

    plat_usb_wait_for_ep_completion(USB_EP3_IN);

    memcpy(recovery_cmd_buffer, bfr, sz);
 
    err = plat_usb_transfer(dev, USB_EP3_IN, recovery_cmd_buffer, sz);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(USB_EP3_IN);

    return PB_OK;
}

static uint32_t recovery_read_data(struct usb_device *dev,
                                   uint8_t *bfr, uint32_t sz)
{
    uint32_t err = PB_OK;

    err = plat_usb_transfer(dev, USB_EP2_OUT, recovery_cmd_buffer, sz);

    if (err != PB_OK)
        return err;

    plat_usb_wait_for_ep_completion(USB_EP2_OUT);

    memcpy(bfr, recovery_cmd_buffer, sz);

    return err;
}

static uint32_t recovery_parse_command(struct usb_device *dev, 
                                       struct usb_pb_command *cmd)
{
    uint32_t err = PB_OK;

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
                return PB_ERR;

            if (cmd_prep.buffer_id > 1)
                return PB_ERR;

            uint8_t *bfr = recovery_bulk_buffer[cmd_prep.buffer_id];

            err = plat_usb_transfer(dev, USB_EP1_OUT, bfr,
                                                cmd_prep.no_of_blocks*512);
        }
        break;
        case PB_CMD_FLASH_BOOTLOADER:
        {
            uint32_t no_of_blks;
            LOG_INFO ("Flash BL %li",no_of_blks);
            recovery_read_data(dev, (uint8_t *) &no_of_blks, 4);
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
            uint32_t sz;

            recovery_read_data(dev, (uint8_t *) data, 8);
            
            LOG_INFO("Set key %lu to %lu", data[0], data[1]);
            config_set_uint32_t(data[0], data[1]);
            config_commit();
        }
        break;
        case PB_CMD_GET_CONFIG_VAL:
        {
            uint32_t config_param = 0;
            uint32_t config_val;

            LOG_INFO("Reading key index, sz=%lu",cmd->size);
            recovery_read_data(dev, (uint8_t *) &config_param, 4);
            config_get_uint32_t(config_param, &config_val);
            err = recovery_send_response(dev, (uint8_t *) &config_val, 4);
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

            recovery_flash_part(wr_part.part_no, 
                                wr_part.lba_offset, 
                                wr_part.no_of_blocks, 
                                recovery_bulk_buffer[wr_part.buffer_id]);
        }
        break;
        case PB_CMD_BOOT_PART:
        {
            //tfp_printf ("\n\rBOOT 1\n\r");
            //if (boot_load(PB_BOOT_A) == PB_OK) {
            //    boot();
            //} else {
            //    tfp_printf ("boot_load failed\n\r");
            //}
            //tfp_printf ("Boot returned\n\r");
        }
        break;
        case PB_CMD_READ_UUID:
        {
            uint8_t board_uuid[16];
            board_get_uuid(board_uuid);
            recovery_send_response(dev, board_uuid, 16);
        }
        break;
        case PB_CMD_WRITE_UUID:
        {
            uint8_t board_uuid[16];
            recovery_read_data(dev, board_uuid, 16);
            board_write_uuid(board_uuid, BOARD_OTP_WRITE_KEY);
        }
        break;
        case PB_CMD_WRITE_DFLT_GPT:
        {
            LOG_INFO ("Installing default GPT table\n\r");
            board_write_gpt_tbl();
        }
        break;
        case PB_CMD_WRITE_DFLT_FUSE:
        {
            LOG_INFO ("Writing default boot fuses\n\r");
            board_write_standard_fuses(BOARD_OTP_WRITE_KEY);
        }
        break;
        default:
            LOG_ERR ("Got unknown command: %lx",cmd->command);
    }

    return err;
}

void recovery_initialize(void)
{
    usb_set_on_command_handler(recovery_parse_command);
}

