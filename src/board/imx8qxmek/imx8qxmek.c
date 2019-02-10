#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <plat.h>
#include <stdbool.h>
#include <usb.h>
#include <fuse.h>
#include <gpt.h>
#include <plat/imx/ehci.h>
#include <plat/imx/usdhc.h>
#include <plat/imx8x/plat.h>

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
    IMX8X_FUSE_ROW(544, "UUID0"),
    IMX8X_FUSE_ROW(545, "UUID1"),
    IMX8X_FUSE_ROW(546, "UUID2"),
    IMX8X_FUSE_ROW(547, "UUID3"),
    IMX8X_FUSE_END,
};

const struct fuse device_info_fuses[] =
{
    IMX8X_FUSE_ROW_VAL(548, "Device Info", 0x12340000),
    IMX8X_FUSE_END,
};

const struct fuse root_hash_fuses[] =
{
    IMX8X_FUSE_ROW(730, "SRK0"),
    IMX8X_FUSE_ROW(731, "SRK1"),
    IMX8X_FUSE_ROW(732, "SRK2"),
    IMX8X_FUSE_ROW(733, "SRK3"),
    IMX8X_FUSE_ROW(734, "SRK4"),
    IMX8X_FUSE_ROW(735, "SRK5"),
    IMX8X_FUSE_ROW(736, "SRK6"),
    IMX8X_FUSE_ROW(737, "SRK7"),
    IMX8X_FUSE_ROW(738, "SRK8"),
    IMX8X_FUSE_ROW(739, "SRK9"),
    IMX8X_FUSE_ROW(740, "SRK10"),
    IMX8X_FUSE_ROW(741, "SRK11"),
    IMX8X_FUSE_ROW(742, "SRK12"),
    IMX8X_FUSE_ROW(743, "SRK13"),
    IMX8X_FUSE_ROW(744, "SRK14"),
    IMX8X_FUSE_ROW(745, "SRK15"),
    IMX8X_FUSE_END,
};


const struct fuse board_fuses[] =
{
    IMX8X_FUSE_ROW_VAL(18, "BOOT Config", 0x00000002),
    IMX8X_FUSE_ROW_VAL(19, "Bootconfig2" , 0x00000025),
    IMX8X_FUSE_END,
};


static struct ehci_device ehcidev = 
{
    .base = 0x5b0d0000,
};

static struct usb_device board_usbdev =
{
    .platform_data = &ehcidev,
};



uint32_t board_early_init(void)
{



    return PB_OK;
}

uint32_t board_late_init(void)
{


    return PB_OK;
}

uint32_t board_configure_gpt_tbl(void)
{
    gpt_add_part(1, 62768,  part_type_system_a, "System A");
    gpt_add_part(2, 62768,  part_type_system_b, "System B");
    gpt_add_part(3, 0x40000, part_type_root_a,   "Root A");
    gpt_add_part(4, 0x40000, part_type_root_b,   "Root B");

    return PB_OK;
}

uint32_t board_get_debug_uart(void)
{
    return 0x0;
}

uint32_t board_usb_init(struct usb_device **usbdev)
{
    (*usbdev) = &board_usbdev;
    return PB_OK;
}

bool board_force_recovery(void)
{
    return true;
}

uint32_t board_configure_bootargs(char *buf, char *boot_part_uuid)
{
    snprintf (buf, 512,"console=ttyLP0,115200  " \
        /*"earlycon=lpuart32,0x5a060000,115200 earlyprintk " \*/
        "quiet " \
        "root=PARTUUID=%s " \
        "ro rootfstype=ext4 gpt rootwait", boot_part_uuid);

    return PB_OK;
}
