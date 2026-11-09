src-y  += src/boot/boot.c
src-$(CONFIG_BOOT_AB_DRIVER) += src/boot/ab_state.c
src-y  += src/boot/image_helpers.c
src-$(CONFIG_BOOT_LINUX) += src/boot/linux.c
src-$(CONFIG_BOOT_ATF) += src/boot/atf.c
ifneq ($(CONFIG_BOOT_LINUX)$(CONFIG_BOOT_ATF),)
src-y  += src/boot/dtb.c
endif
src-$(CONFIG_BOOT_SIMPLE) += src/boot/simple.c
src-$(CONFIG_BOOT_LINUX_RAMOOPS) += src/boot/linux_ramoops.c
