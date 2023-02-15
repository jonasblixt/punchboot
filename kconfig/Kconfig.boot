menu "Boot configuration"
comment "Boot configuration"

choice BOOT
    bool "Select boot method"
config BOOT_AB
    bool "A/B boot"
endchoice

config BOOT_IMAGE_ID
    hex "Boot image id"
    default 0x00000000

config BOOT_STATE_PRIMARY_UU
    string "Boot state primary partition UUID"
    default "f5f8c9ae-efb5-4071-9ba9-d313b082281e"

config BOOT_STATE_BACKUP_UU
    string "Boot state backup partition UUID"
    default "656ab3fc-5856-4a5e-a2ae-5a018313b3ee"

config BOOT_DT
    bool "Enable device tree"

config BOOT_DT_ID
    hex "Device tree image id"
    depends on BOOT_DT

config BOOT_RAMDISK
    bool "Enable ramdisk"

config BOOT_RAMDISK_ID
    hex "Ramdisk image id"
    depends on BOOT_RAMDISK

config BOOT_ENABLE_DTB_BOOTARG
    bool "Pass argument to kernel with device tree address"
    depends on BOOT_RAMDISK

config BOOT_AB_A_UUID
    string "A partition UUID"
    depends on BOOT_AB

config BOOT_AB_B_UUID
    string "B partition UUID"
    depends on BOOT_AB

config BOOT_POP_TIMING
    bool "Include timestamp in device-tree"
    depends on BOOT_DT
    default n

config BOOT_ROLLBACK_MODE_SPECULATIVE
    bool "Speculative rollback mode"
    depends on BOOT_AB
    help
        In speculative rollback mode the bootloader will alternate between A
        and B paritions until runtime software can either perform an update
        or set the verified bit on either A or B.

config CALL_EARLY_PLAT_BOOT
    bool "Call early/late plat/board boot callbacks"
    default n

endmenu
