/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <config.h>
#include <plat.h>
#include <gpt.h>
#include <crc.h>
#include <tinyprintf.h>

static uint8_t _flag_config_ok = false;
static uint32_t _config_lba_offset = 0;

static const struct pb_config_item _pb_config[] = 
{
    /* Index                , Description     , Access              , Default */
    {  PB_CONFIG_BOOT       , "Boot Partition", PB_CONFIG_ITEM_RO   , 0xAA},
    {  PB_CONFIG_BOOT_COUNT , "Boot Count"    , PB_CONFIG_ITEM_RO   , 0},
    {  PB_CONFIG_FORCE_RECOVERY, "Force Recovery", PB_CONFIG_ITEM_RW, 0},
    {  -1                   , ""              , 0                   , 0 },
};

static const uint32_t _config_tot_size = 
                    (sizeof(_pb_config) / sizeof(struct pb_config_item) -1)*4;

static const uint32_t _config_no_of_items = 
                    (sizeof(_pb_config) / sizeof(struct pb_config_item))-1;


__a4k static struct pb_config_data _config_data;

uint32_t config_init(void) 
{
    uint32_t n = 0;
    uint32_t err = PB_OK;

    
    err = gpt_get_part_by_uuid(part_type_config, &_config_lba_offset);

    if (err != PB_OK) 
    {
        LOG_INFO ("Could not find config partition");
        _flag_config_ok = false;
        return PB_ERR;
    }

    plat_read_block(_config_lba_offset, (uintptr_t) &_config_data, 1);


    if ((crc32 (0, (uint8_t *)_config_data.data, _config_tot_size) 
            == _config_data.crc) && (_config_data._magic == PB_CONFIG_MAGIC) ) 
    {
       LOG_INFO ("Found valid config, %"PRIu32" bytes", _config_tot_size);
    } else {
        LOG_WARN ("CRC 0x%8.8"PRIx32, _config_data.crc);
        LOG_WARN ("Corrupt config, installing default...");
        n = 0;
        do
        {
            _config_data.data[n] = _pb_config[n].default_value;
            n++;
        } while (_pb_config[n].index > 0);

        config_commit();
    }

    _flag_config_ok = true;
    return PB_OK;
}

uint8_t config_ok(void) 
{
    return _flag_config_ok;
}

struct pb_config_item * config_get_tbl(void) 
{
    return (struct pb_config_item *) _pb_config;
}

uint32_t config_get_tbl_sz(void) 
{
    return sizeof(_pb_config);
}

uint32_t config_get_uint32_t(uint8_t index, uint32_t *value) 
{
    if ( (index > 127) || (index >= _config_no_of_items))
    {
        LOG_ERR ("Out of range index (%u)",index);
        return PB_ERR;
    }

    *(value) = _config_data.data[index];

    return PB_OK;
}

uint32_t config_set_uint32_t(uint8_t index, uint32_t value) 
{
    
    if ( (index > 127) || (index >= _config_no_of_items))
    {
        LOG_ERR ("Out of range index (%u)",index);
        return PB_ERR;
    }
    _config_data.data[index] = value;
    return PB_OK;
}

uint32_t config_commit(void) 
{
    _config_data._magic = PB_CONFIG_MAGIC;

    _config_data.crc = 
                crc32(0, (uint8_t *) _config_data.data, _config_tot_size);

    plat_write_block(_config_lba_offset, (uintptr_t) &_config_data, 1);

    LOG_INFO ("LBA offset = 0x%8.8"PRIx32, _config_lba_offset);
    LOG_INFO ("Wrote %"PRIu32" bytes, CRC: 0x%8.8"PRIx32, _config_tot_size,
                                                _config_data.crc);

    return PB_OK;
}

