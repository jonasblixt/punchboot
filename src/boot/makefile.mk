src-y  += src/boot/boot.c
src-y  += src/boot/policy.c
src-$(CONFIG_BOOT_POLICY_FIXED) += src/boot/policy_fixed.c
src-$(CONFIG_BOOT_POLICY_CHAIN) += src/boot/policy_chain.c
src-$(CONFIG_BOOT_POLICY_AB) += src/boot/ab_state.c
src-y  += src/boot/image_helpers.c
src-$(CONFIG_BOOT_LINUX) += src/boot/image.c
src-$(CONFIG_BOOT_ATF) += src/boot/atf.c
src-$(CONFIG_BOOT_DTB) += src/boot/dtb.c
src-$(CONFIG_BOOT_FH) += src/boot/fh.c

# The transfer list's region reaches the linker too (platform.md 5.1), which
# keeps pb and its buffers off it (fh.lds)
ifdef CONFIG_BOOT_FH
ldflags-y += --defsym=BOOT_FH_REGION_BASE=$(CONFIG_BOOT_FH_REGION_BASE)
ldflags-y += --defsym=BOOT_FH_REGION_SIZE=$(CONFIG_BOOT_FH_REGION_SIZE)
ldflags-y += -Tsrc/boot/fh.lds
endif
src-$(CONFIG_BOOT_LINUX_RAMOOPS) += src/boot/linux_ramoops.c
