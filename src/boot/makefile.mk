src-y  += src/boot/boot.c
src-$(CONFIG_BOOT_AB_DRIVER) += src/boot/ab_state.c
src-y  += src/boot/image_helpers.c
src-$(CONFIG_BOOT_LINUX) += src/boot/linux.c
src-$(CONFIG_BOOT_ATF) += src/boot/atf.c
src-$(CONFIG_BOOT_ARMV7M_BAREMETAL) += src/boot/armv7m_baremetal.c
