#ifndef __PLAT_H__
#define __PLAT_H__

#include "pb_types.h"

#define PLAT_EMMC_PART_BOOT0 1
#define PLAT_EMMC_PART_BOOT1 2
#define PLAT_EMMC_PART_USER  0

typedef u32 t_plat_usb_cb(u8*, u8*, u8*);

void plat_reset(void);
u32  plat_get_ms_tick(void);

/* EMMC Interface */
u32  plat_emmc_write_block(u32 lba_offset, u8 *bfr, u32 no_of_blocks);
u32  plat_emmc_read_block(u32 lba_offset, u8 *bfr, u32 no_of_blocks);
u32  plat_emmc_switch_part(u8 part_no);

/* Crypto Interface */
u32  plat_sha256_init(void);
u32  plat_sha256_update(u8 *bfr, u32 sz);
u32  plat_sha256_finalize(u8 *out);
u32  plat_rsa_enc(u8 *sig, u32 sig_sz, u8 *out, 
                    u8 *pk_modulus, u32 pk_modulus_sz,
                    u8 *pk_exponent, u32 pk_exponent_sz);

/* USB Interface */
u32  plat_usb_cmd_callback(t_plat_usb_cb *cb);
u32  plat_usb_send(u8 *bfr, u32 sz);
u32  plat_usb_prep_bulk_buffer(u16 no_of_blocks, u8 buffer_id);
void plat_usb_task(void);

/* UART Interface */
void plat_uart_putc(u8 *ptr, u8 c);


#endif
