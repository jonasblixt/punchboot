#include <pb.h>
#include <io.h>
#include <plat.h>
#include <stdbool.h>
#include <usb.h>
#include <fuse.h>

#include <plat/imx8m/plat.h>

const uint8_t part_type_config[] = 
{
    0xF7, 0xDD, 0x45, 0x34, 0xCC, 0xA5, 0xC6, 0x45, 
    0xAA, 0x17, 0xE4, 0x10, 0xA5, 0x42, 0xBD, 0xB8
};

const uint8_t part_type_system_a[] = 
{
    0x59, 0x04, 0x49, 0x1E, 0x6D, 0xE8, 0x4B, 0x44, 
    0x82, 0x93, 0xD8, 0xAF, 0x0B, 0xB4, 0x38, 0xD1
};

const uint8_t part_type_system_b[] = 
{ 
    0x3C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};

const uint8_t part_type_root_a[] = 
{ 
    0x1C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};

const uint8_t part_type_root_b[] = 
{ 
    0x2C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};


const struct fuse uuid_fuses[] =
{
    IMX8M_FUSE_BANK_WORD(15, 4, "UUID0"),
    IMX8M_FUSE_BANK_WORD(15, 5, "UUID1"),
    IMX8M_FUSE_BANK_WORD(15, 6, "UUID2"),
    IMX8M_FUSE_BANK_WORD(15, 7, "UUID3"),
    IMX8M_FUSE_END,
};

const struct fuse device_info_fuses[] =
{
    IMX8M_FUSE_BANK_WORD_VAL(15, 3, "Device Info", 0x12340000),
    IMX8M_FUSE_END,
};

const struct fuse root_hash_fuses[] =
{
    IMX8M_FUSE_BANK_WORD(3, 0, "SRK0"),
    IMX8M_FUSE_BANK_WORD(3, 1, "SRK1"),
    IMX8M_FUSE_BANK_WORD(3, 2, "SRK2"),
    IMX8M_FUSE_BANK_WORD(3, 3, "SRK3"),
    IMX8M_FUSE_BANK_WORD(3, 4, "SRK4"),
    IMX8M_FUSE_BANK_WORD(3, 5, "SRK5"),
    IMX8M_FUSE_BANK_WORD(3, 6, "SRK6"),
    IMX8M_FUSE_BANK_WORD(3, 7, "SRK7"),
    IMX8M_FUSE_END,
};


const struct fuse board_fuses[] =
{
    IMX8M_FUSE_BANK_WORD_VAL(0, 5, "BOOT Config",        0x0000c060),
    IMX8M_FUSE_BANK_WORD_VAL(0, 6, "BOOT from fuse bit", 0x00000010),
    IMX8M_FUSE_END,
};


uint32_t board_init(void)
{

    /* Enable UART1 clock */
    pb_write32((1 << 28) ,0x30388004 + 94*0x80);
    /* Ungate UART1 clock */
    pb_write32(3, 0x30384004 + 0x10*73);

    /* UART1 pad mux */
    pb_write32(0, 0x30330234);
    pb_write32(0, 0x30330238);
    
    /* UART1 PAD settings */
    pb_write32(7, 0x3033049C);
    pb_write32(7, 0x303304A0);


    return PB_OK;
}

uint32_t board_configure_gpt_tbl(void)
{
    return PB_ERR;
}

uint32_t board_get_debug_uart(void)
{
    return 0x30860000;
}

uint32_t board_usb_init(struct usb_device **usbdev)
{
    return PB_ERR;
}

bool board_force_recovery(void)
{
    return true;
}
