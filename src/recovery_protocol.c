#include <pb.h>
#include <recovery_protocol.h>

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
    "PB_CMD_WRITE_UUID",
    "PB_CMD_READ_UUID",
    "PB_CMD_WRITE_DFLT_GPT",
    "PB_CMD_WRITE_DFLT_FUSE",
};
