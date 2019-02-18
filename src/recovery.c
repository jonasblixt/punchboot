/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb.h>
#include <stdio.h>
#include <recovery.h>
#include <image.h>
#include <plat.h>
#include <usb.h>
#include <keys.h>
#include <io.h>
#include <board.h>
#include <gpt.h>
#include <string.h>
#include <boot.h>
#include <board/config.h>
#include <plat/defs.h>

#define RECOVERY_CMD_BUFFER_SZ  1024*64
#define RECOVERY_BULK_BUFFER_SZ 1024*1024*8

static uint8_t __a4k __no_bss recovery_cmd_buffer[RECOVERY_CMD_BUFFER_SZ];
static uint8_t __a4k __no_bss recovery_bulk_buffer[2][RECOVERY_BULK_BUFFER_SZ];
static __no_bss __a4k struct pb_pbi pbi;
static struct gpt *gpt;
extern const struct fusebox pb_fusebox;
extern const struct partition_table pb_partition_table[];

extern char _code_start, _code_end, _data_region_start, _data_region_end, 
            _zero_region_start, _zero_region_end, _stack_start, _stack_end;

const char *recovery_cmd_name[] =
{
    "PB_CMD_RESET",
    "PB_CMD_FLASH_BOOTLOADER",
    "PB_CMD_PREP_BULK_BUFFER",
    "PB_CMD_GET_VERSION",
    "PB_CMD_GET_GPT_TBL",
    "PB_CMD_WRITE_PART",
    "PB_CMD_BOOT_PART",
    "PB_CMD_BOOT_ACTIVATE",
    "PB_CMD_WRITE_DFLT_GPT",
    "PB_CMD_BOOT_RAM",
    "PB_CMD_SETUP",
    "PB_CMD_SETUP_LOCK",
    "PB_CMD_GET_HW_INFO",
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
    plat_write_block(PB_BOOTPART_OFFSET, (uintptr_t) bfr, blocks_to_write);

    plat_switch_part(PLAT_EMMC_PART_BOOT1);
    plat_write_block(PB_BOOTPART_OFFSET, (uintptr_t) bfr, blocks_to_write);

    plat_switch_part(PLAT_EMMC_PART_USER);
 
    return PB_OK;
}

static uint32_t recovery_flash_part(uint8_t part_no, 
                                    uint32_t lba_offset, 
                                    uint32_t no_of_blocks, 
                                    uint8_t *bfr) 
{
    uint32_t part_lba_offset = 0;

    part_lba_offset = gpt_get_part_first_lba(gpt, part_no);

    if (!part_lba_offset) 
    {
        LOG_ERR ("Unknown partition");
        return PB_ERR;
    }

    if ( (lba_offset + no_of_blocks) > gpt_get_part_last_lba(gpt,part_no))
    {
        LOG_ERR ("Trying to write outside of partition");
        return PB_ERR;
    }
    
    return plat_write_block(part_lba_offset + lba_offset, 
                                (uintptr_t) bfr, no_of_blocks);
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
    uint32_t err;

    UNUSED(dev);

    /* Read fuses */
    foreach_fuse(f, (struct fuse *) pb_fusebox.fuses)
    {
        err = plat_fuse_read(f);
 
        LOG_DBG("Fuse %s: 0x%08x",f->description,f->value);
        if (err != PB_OK)
        {
            LOG_ERR("Could not access fuse '%s'",f->description);
            return err;
        }  
    }

    /* Check device identity */
    struct fuse *ident = (struct fuse *) &pb_fusebox.identity;
    err = plat_fuse_read(ident);
    
    if (err != PB_OK)
    {
        LOG_ERR("Could not access device identity fuse");
        return err;
    }  

    if (pb_fusebox.identity.value != 0)
    {
        LOG_ERR("Device identity already set");
        return PB_ERR;
    }

    ident->value = ident->default_value;

    ident->value |= ((pb_setup->device_variant << 8) & 0xff00) |
                    ((pb_setup->device_revision) & 0xff);

    /* Perform the actual fuse programming */
    
    LOG_INFO("Writing fuses");

    foreach_fuse(f, pb_fusebox.fuses)
    {
        f->value = f->default_value;
        err = plat_fuse_write(f);

        if (err != PB_OK)
            return err;
    }

    err = plat_fuse_write((struct fuse *) &pb_fusebox.identity);

    return PB_OK;
}

static void recovery_parse_command(struct usb_device *dev, 
                                       struct pb_cmd_header *cmd)
{
    uint32_t err = PB_OK;

    LOG_INFO ("0x%x %s, sz=%ub", cmd->cmd, 
                                      recovery_cmd_name[cmd->cmd],
                                      cmd->size);

    switch (cmd->cmd) 
    {
        case PB_CMD_PREP_BULK_BUFFER:
        {
            struct pb_cmd_prep_buffer cmd_prep;

            recovery_read_data(dev, (uint8_t *) &cmd_prep,
                                sizeof(struct pb_cmd_prep_buffer));
 
            LOG_INFO("Preparing buffer %u [%u]",
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
            LOG_INFO ("Flash BL %u",cmd->arg0);
            recovery_flash_bootloader(recovery_bulk_buffer[0], 
                        cmd->arg0);
        }
        break;
        case PB_CMD_GET_VERSION:
        {
            char version_string[20];

            LOG_INFO ("Get version");
            snprintf(version_string, 19, "PB %s",VERSION);

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
                __asm__ volatile ("wfi");
        }
        break;
        case PB_CMD_GET_GPT_TBL:
        {
            err = recovery_send_response(dev,(uint8_t*) &gpt->primary,
                                        sizeof (struct gpt_primary_tbl));  
        }
        break;
        case PB_CMD_BOOT_ACTIVATE:
        {
            struct gpt_part_hdr *part_sys_a, *part_sys_b;

            gpt_get_part_by_uuid(gpt, PB_PARTUUID_SYSTEM_A, &part_sys_a);
            gpt_get_part_by_uuid(gpt, PB_PARTUUID_SYSTEM_B, &part_sys_b);

            if (cmd->arg0 == 0)
            {
                gpt_part_set_bootable(part_sys_a, false);
                gpt_part_set_bootable(part_sys_b, false);
            }

            if ((cmd->arg0 & 1) == 1)
            {   
                LOG_INFO("Activating System A");
                gpt_part_set_bootable(part_sys_a, true);
                gpt_part_set_bootable(part_sys_b, false);
            }

            if ((cmd->arg0 & 2) == 2)
            {
                LOG_INFO("Activating System B");
                gpt_part_set_bootable(part_sys_a, false);
                gpt_part_set_bootable(part_sys_b, true);
            }

            err = gpt_write_tbl(gpt);
            LOG_INFO("Result %u",err);
        }
        break;
        case PB_CMD_WRITE_PART:
        {
            struct pb_cmd_write_part wr_part;
        
            recovery_read_data(dev, (uint8_t *) &wr_part,
                                    sizeof(struct pb_cmd_write_part));
            
            LOG_INFO ("Writing %u blks to part %u" \
                        " with offset %x using bfr %u",
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
            struct gpt_part_hdr *boot_part_a, *boot_part_b;

            err = gpt_get_part_by_uuid(gpt, PB_PARTUUID_SYSTEM_A, &boot_part_a);
            err = gpt_get_part_by_uuid(gpt, PB_PARTUUID_SYSTEM_B, &boot_part_b);

            if (cmd->arg0 == 1)
            {
                LOG_INFO("Loading System A");
                err = pb_image_load_from_fs(boot_part_a->first_lba, &pbi);
            } else if (cmd->arg0 == 2) {
                LOG_INFO("Loading System B");
                err = pb_image_load_from_fs(boot_part_b->first_lba, &pbi);
            } else {
                LOG_ERR("Invalid boot partition");
                err = PB_ERR;
            }
            
            if (pb_image_verify(&pbi) == PB_OK)
            {
                LOG_INFO("Booting image...");
                recovery_send_result_code(dev, err);
                pb_boot(&pbi, SYSTEM_A);
            } else {
                LOG_ERR("Image verification failed");
                err = PB_ERR;
            }

            recovery_send_result_code(dev, err);
        }

        break;
        case PB_CMD_BOOT_RAM:
        {   
            recovery_read_data(dev, (uint8_t *) &pbi, sizeof(struct pb_pbi));

            err = pb_image_check_header(&pbi);

            if (err != PB_OK)
                break;

            recovery_send_result_code(dev, err);

            if (err != PB_OK)
                break;

            for (uint32_t i = 0; i < pbi.hdr.no_of_components; i++) 
            {
                LOG_INFO("Loading component %u, %u bytes",i, 
                                        pbi.comp[i].component_size);

                err = plat_usb_transfer(dev, USB_EP1_OUT,
                        (uint8_t *)(uintptr_t) pbi.comp[i].load_addr_low,
                        pbi.comp[i].component_size );

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

            if (pb_image_verify(&pbi) == PB_OK)
            {
                LOG_INFO("Booting image...");
                recovery_send_result_code(dev, err);
                pb_boot(&pbi, SYSTEM_A);
            } else {
                LOG_ERR("Image verification failed");
                err = PB_ERR;
            }

        }
        break;
        case PB_CMD_WRITE_DFLT_GPT:
        {
            LOG_INFO ("Installing default GPT table");

            err = gpt_init_tbl(gpt, 1, plat_get_lastlba());

            if (err != PB_OK)
                break;

            uint32_t part_count = 0;
            for (const struct partition_table *p = pb_partition_table;
                    (p->no_of_blocks != 0); p++)
            {
                err = gpt_add_part(gpt, part_count++, p->no_of_blocks,
                                                 p->uuid,
                                                 p->name);

                if (err != PB_OK)
                    break;
            }

            if (err != PB_OK)
                break;

            err = gpt_write_tbl(gpt);

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
            err = plat_setup_lock();
        }
        break;
        case PB_CMD_GET_HW_INFO:
        {
            struct fuse *f = (struct fuse *) &pb_fusebox.identity;

            plat_fuse_read(f);
            recovery_send_response(dev,(uint8_t*) &f->value,
                                        sizeof (uint32_t));  
            err = PB_OK;
        }
        break;
        default:
            LOG_ERR ("Got unknown command: %u",cmd->cmd);
    }
    
    
    recovery_send_result_code(dev, err);
}

void recovery_initialize(struct gpt *_gpt)
{
    gpt = _gpt;
    LOG_DBG("array crc %x", gpt->primary.hdr.part_array_crc);
    LOG_DBG("GPT ptr %p",gpt);
    usb_set_on_command_handler(recovery_parse_command);
}

