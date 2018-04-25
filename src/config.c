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

#undef CONFIG_DEBUG

static u8 _flag_config_ok = false;
static u32 _config_lba_offset = 0;

static const struct pb_config_item _pb_config[] = {
    /* Index                , Description     , Access              , Default */
    {  PB_CONFIG_BOOT       , "Boot Partition", PB_CONFIG_ITEM_RO   , 0xAA},
    {  PB_CONFIG_BOOT_COUNT , "Boot Count"    , PB_CONFIG_ITEM_RO   , 0},
    {  -1                   , ""              , 0                   , 0 },
};

static const u32 _config_tot_size = 
                    (sizeof(_pb_config) / sizeof(struct pb_config_item) -1)*4;


static struct pb_config_data _config_data;

u32 config_init(void) {
    u32 n = 0;
    s8 part_id = gpt_get_part_by_uuid(part_type_config);

#ifdef CONFIG_DEBUG
    if (part_id >= 0)
        tfp_printf ("Config: loading from part %i, %i bytes\n\r", part_id,
                                    _config_tot_size);
#endif
    if (part_id < 0) {
        tfp_printf ("Config: Could not find config partition\n\r");
        _flag_config_ok = false;
        return PB_ERR;
    }

    _config_lba_offset = gpt_get_part_offset(part_id);

    plat_emmc_read_block(_config_lba_offset, (u8 *) &_config_data, 1);


    if ((crc32 (0, (u8 *)_config_data.data, _config_tot_size) 
            == _config_data.crc) && (_config_data._magic == PB_CONFIG_MAGIC) ) {
#ifdef CONFIG_DEBUG
       tfp_printf ("Config: Found valid config, %i bytes\n\r", 
                        _config_tot_size);
#endif
    } else {
        tfp_printf ("Config: CRC 0x%8.8X\n\r", _config_data.crc);
        tfp_printf ("Config: Corrupt config, installing default...\n\r");
        n = 0;
        do {
            _config_data.data[n] = _pb_config[n].default_value;
            n++;
        } while (_pb_config[n].index > 0);

        config_commit();
    }



    _flag_config_ok = true;
    return PB_OK;
}

u8 config_ok(void) {
    return _flag_config_ok;
}

u8* config_get_tbl(void) {
    return (u8 *) _pb_config;
}

u32 config_get_tbl_sz(void) {
    return sizeof(_pb_config);
}

u32 config_get_u32(u8 index, u32 *value) {
    /* TODO: Out of range check */
    *(value) = _config_data.data[index & 0x7f];
    return PB_OK;
}

u32 config_set_u32(u8 index, u32 value) {
    _config_data.data[index & 0x7f] = value;
    return PB_OK;
}

u32 config_commit(void) {
    _config_data._magic = PB_CONFIG_MAGIC;

    _config_data.crc = crc32(0, (u8 *) _config_data.data, _config_tot_size);
    plat_emmc_write_block(_config_lba_offset, (u8 *) &_config_data, 1);

#ifdef CONFIG_DEBUG
    tfp_printf ("Config: LBA offset = 0x%8.8X\n\r", _config_lba_offset);
    tfp_printf ("Config: Wrote %i bytes, CRC: 0x%8.8X\n\r", _config_tot_size,
                                                            _config_data.crc);
#endif      

    return PB_OK;
}
