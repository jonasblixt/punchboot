/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

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
#define REPORT_SZ 1024*10
static uint8_t __a4k __no_bss recovery_cmd_buffer[RECOVERY_CMD_BUFFER_SZ];
static uint8_t __a4k __no_bss recovery_bulk_buffer[2][RECOVERY_BULK_BUFFER_SZ];
static char __no_bss report_text_buffer[REPORT_SZ];

extern const struct fuse device_info_fuses[];
extern const struct fuse root_hash_fuses[];
extern const struct fuse board_fuses[];
extern const uint32_t build_root_hash[];


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

    bool flag_root_hash_fused = false;
    bool flag_devid_fused = false;
    bool flag_devid_revvar_fused = false;
    bool flag_board_fused = true;


    /* Root hash */
    foreach_fuse_read(f, root_hash_fuses)
    {
        if (f->value != 0)
            flag_root_hash_fused = true;
    }

    if (!flag_root_hash_fused)
    {
        uint32_t *root_hash_part = (uint32_t *) build_root_hash;

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
        pos += snprintf(&report_text_buffer[pos], REPORT_SZ-pos,
                                    "-- Device setup report --\n");


        /* Root hash */
        if (!flag_root_hash_fused)
        {
            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Will write root key hash:\n");
        } else {
            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Root key hash already fused:\n");
        }

        foreach_fuse(f, root_hash_fuses)
            pos += plat_fuse_to_string(f, &report_text_buffer[pos], 64);

        /* Device identity */

        if (!flag_devid_fused)
        {
            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Will write device identity: %x\n", 
                            devid->default_value >> 16);
        } else {

            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Device identity already fused: 0x%08x\n", 
                            devid->value >> 16);

            if ( (devid->value & 0xFFFF0000) != devid->default_value)
            {
                pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                            "  WARNING: device id mismatch with build in value\n");

                pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                            "    0x%x != 0x%x\n", 
                                (devid->value & 0xFFFF0000), 
                                devid->default_value);
            }
        }

        if (!flag_devid_revvar_fused)
        {
            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Will write device variant: %x, rev: %x\n", 
                            (uint8_t) pb_setup->device_variant,
                            (uint8_t) pb_setup->device_revision);
        } else {

            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Device var/rev already fused, var: %x, rev: %x \n", 
                            (uint8_t) ((devid->value >> 8) & 0xff),
                            (uint8_t) (devid->value & 0xff));
        }

        pos += plat_fuse_to_string(devid, &report_text_buffer[pos], 64);

        /* Board fuses */
        if (!flag_board_fused)
        {
            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Will write board fuses\n");
        } else {

            pos += snprintf(&report_text_buffer[pos],REPORT_SZ-pos,
                        "Board fuses already fused\n");
        }

        foreach_fuse(f, board_fuses)
            pos += plat_fuse_to_string(f, &report_text_buffer[pos], 64);

        report_text_buffer[pos] = 0;
        recovery_send_response(dev, (uint8_t *) report_text_buffer, pos+1);
    } else {
        /* Perform the actual fuse programming */

        
        if (!flag_root_hash_fused)
        {
            LOG_INFO("Writing root hash fuses");
            foreach_fuse(f, root_hash_fuses)
            {
                err = plat_fuse_write(f);
                if (err != PB_OK)
                    return err;
            }
        }

        if (!flag_devid_fused || !flag_devid_revvar_fused)
        {
            LOG_INFO("Writing device id fuses");
            err = plat_fuse_write(devid);

                if (err != PB_OK)
                    return err;
        }

        if (!flag_board_fused)
        {
            foreach_fuse_read(f, board_fuses)
            {

                if ((f->value & f->default_value) != f->default_value)
                {
                    f->value = f->default_value;
                    err = plat_fuse_write(f);

                    if (err != PB_OK)
                        return err;
                }
            }
        }
    }


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
            snprintf(version_string, 20, "PB %s",VERSION);

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
            err = recovery_send_response(dev,((uint8_t*) gpt_get_tbl()), 
                            sizeof (struct gpt_primary_tbl));          
        }
        break;
        case PB_CMD_BOOT_ACTIVATE:
        {
            struct gpt_part_hdr *part_sys_a, *part_sys_b;
            uint64_t *attr_a, *attr_b;
            gpt_get_part_by_uuid(part_type_system_a, &part_sys_a);
            gpt_get_part_by_uuid(part_type_system_b, &part_sys_b);

            if (cmd->arg0 == 0)
            {
                attr_a = (uint64_t *) part_sys_a->attr;
                (*attr_a) &= ~GPT_ATTR_BOOTABLE;

                attr_b = (uint64_t *) part_sys_b->attr;
                (*attr_b) &= ~GPT_ATTR_BOOTABLE;
            }

            if ((cmd->arg0 & 1) == 1)
            {   
                LOG_INFO("Activating System A");
                attr_a = (uint64_t *) part_sys_a->attr;
                (*attr_a) |= GPT_ATTR_BOOTABLE;

                attr_b = (uint64_t *) part_sys_b->attr;
                (*attr_b) &= ~GPT_ATTR_BOOTABLE;
            }

            if ((cmd->arg0 & 2) == 2)
            {
                LOG_INFO("Activating System B");
                attr_a = (uint64_t *) part_sys_b->attr;
                (*attr_a) &= ~GPT_ATTR_BOOTABLE;

                attr_b = (uint64_t *) part_sys_b->attr;
                (*attr_b) |= GPT_ATTR_BOOTABLE;
            }

            err = gpt_write_tbl();
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
            struct pb_pbi *pbi = NULL;
            struct gpt_part_hdr *boot_part_a, *boot_part_b;

            err = gpt_get_part_by_uuid(part_type_system_a, &boot_part_a);
            err = gpt_get_part_by_uuid(part_type_system_b, &boot_part_b);

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
            
            recovery_send_result_code(dev, err);

            if (err != PB_OK)
            {
                LOG_ERR("Unable to load image");
                break;
            }

#ifdef PB_BOOT_LINUX
                pb_boot_linux_with_dt(pbi);
#elif PB_BOOT_TEST
                plat_reset();
#endif
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
                LOG_INFO (" o %u - LA: 0x%x OFF:0x%x",
                                    i, 
                                    pbi->comp[i].load_addr_low,
                                    pbi->comp[i].component_offset);


                uintptr_t la = pbi->comp[i].load_addr_low;
                uint32_t sz = pbi->comp[i].component_size;

                if (PB_CHECK_OVERLAP(la,sz,&_stack_start,&_stack_end))
                {
                    LOG_ERR("image overlapping with PB stack");
                    err = PB_ERR;
                    break;
                }

                if (PB_CHECK_OVERLAP(la,sz,&_data_region_start,&_data_region_end))
                {
                    LOG_ERR("image overlapping with PB data");
                    err = PB_ERR;
                    break;
                }


                if (PB_CHECK_OVERLAP(la,sz,&_zero_region_start,&_zero_region_end))
                {
                    LOG_ERR("image overlapping with PB bss");
                    err = PB_ERR;
                    break;
                }

                if (PB_CHECK_OVERLAP(la,sz,&_code_start,&_code_end))
                {
                    LOG_ERR("image overlapping with PB code");
                    err = PB_ERR;
                    break;
                }

            }

            recovery_send_result_code(dev, err);

            if (err != PB_OK)
                break;

            for (uint32_t i = 0; i < pbi->hdr.no_of_components; i++) 
            {
                LOG_INFO("Loading component %u, %u bytes",i, 
                                        pbi->comp[i].component_size);

                err = plat_usb_transfer(dev, USB_EP1_OUT,
                        (uint8_t *)(uintptr_t) pbi->comp[i].load_addr_low,
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

#ifdef PB_BOOT_LINUX
                pb_boot_linux_with_dt(pbi);
#elif PB_BOOT_TEST
                plat_reset();
#endif
            } else {
                LOG_ERR("Image verification failed");
                err = PB_ERR;
            }

        }
        break;
        case PB_CMD_WRITE_DFLT_GPT:
        {
            LOG_INFO ("Installing default GPT table");

            err = gpt_init_tbl(1, plat_get_lastlba());

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
            err = plat_setup_lock();
        }
        break;
        default:
            LOG_ERR ("Got unknown command: %u",cmd->cmd);
    }
    
    
    recovery_send_result_code(dev, err);
}

void recovery_initialize(void)
{
    usb_set_on_command_handler(recovery_parse_command);
}

