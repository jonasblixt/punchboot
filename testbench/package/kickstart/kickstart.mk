################################################################################
#
# kickstart
#
################################################################################

KICKSTART_VERSION := $(call pb_testbench_git_head,$(PB_TESTBENCH_KICKSTART_REPO),$(PB_TESTBENCH_KICKSTART_BRANCH))
KICKSTART_SITE = $(PB_TESTBENCH_KICKSTART_REPO)
KICKSTART_SITE_METHOD = git
# No .hash file: the tarball of a branch head changes with every commit,
# Buildroot only warns about a package without one
KICKSTART_LICENSE = BSD-3-Clause
KICKSTART_LICENSE_FILES = LICENSE
KICKSTART_DEPENDENCIES = bpak mbedtls host-bpak host-python3
KICKSTART_INSTALL_TARGET = NO
KICKSTART_INSTALL_IMAGES = YES

# The default keystore: punchboot's test key (bpak generate needs a
# package id), under the name of the
# punchboot key, so that KICKSTART_REQUIRE_PB_KEYS finds it in
# /chosen/pb,slc-available-keys
KICKSTART_KEYSTORE = $(call qstrip,$(BR2_PACKAGE_KICKSTART_KEYSTORE))
ifeq ($(KICKSTART_KEYSTORE),)
KICKSTART_KEYSTORE = $(@D)/test-keystore.bpak
KICKSTART_TEST_KEY = $(PB_TESTBENCH_PUNCHBOOT_SRCDIR)/pki/secp256r1-pub-key.pem

define KICKSTART_TEST_KEYSTORE
	rm -f $(KICKSTART_KEYSTORE)
	$(HOST_DIR)/bin/bpak create $(KICKSTART_KEYSTORE) -Y
	$(HOST_DIR)/bin/bpak add $(KICKSTART_KEYSTORE) --meta bpak-package \
		--from-string a4c0ce0f-80ab-4159-bcde-85f9a52290ad --encoder uuid
	$(HOST_DIR)/bin/bpak add $(KICKSTART_KEYSTORE) --meta keystore-provider-id \
		--from-string kickstart --encoder id
	$(HOST_DIR)/bin/bpak add $(KICKSTART_KEYSTORE) --part pb-development \
		--from-file $(KICKSTART_TEST_KEY) --encoder key
endef
KICKSTART_PRE_CONFIGURE_HOOKS += KICKSTART_TEST_KEYSTORE
endif

KICKSTART_CONF_OPTS = \
	-DCMAKE_BUILD_TYPE=MinSizeRel \
	-DKICKSTART_ROOT_A="$(call qstrip,$(BR2_PACKAGE_KICKSTART_ROOT_A))" \
	-DKICKSTART_ROOT_B="$(call qstrip,$(BR2_PACKAGE_KICKSTART_ROOT_B))" \
	-DKICKSTART_PBSTATE_PRIMARY="$(call qstrip,$(BR2_PACKAGE_KICKSTART_PBSTATE_PRIMARY))" \
	-DKICKSTART_PBSTATE_BACKUP="$(call qstrip,$(BR2_PACKAGE_KICKSTART_PBSTATE_BACKUP))" \
	-DKICKSTART_KEYSTORE=$(KICKSTART_KEYSTORE) \
	-DKICKSTART_LOG_LEVEL=$(call qstrip,$(BR2_PACKAGE_KICKSTART_LOG_LEVEL)) \
	-DKICKSTART_BPAK_TOOL=$(HOST_DIR)/bin/bpak

# The init goes into the initramfs, not the root file system. mkroot.sh
# creates the root images, post-image uses it.
define KICKSTART_INSTALL_IMAGES_CMDS
	$(TARGET_STRIP) -o $(KICKSTART_BUILDDIR)/kickstart.stripped $(KICKSTART_BUILDDIR)/kickstart
	$(HOST_DIR)/bin/python3 $(@D)/scripts/mkinitramfs.py \
		--init $(KICKSTART_BUILDDIR)/kickstart.stripped -o $(BINARIES_DIR)/kickstart.cpio.gz
	$(INSTALL) -D -m 0755 $(@D)/scripts/mkroot.sh $(HOST_DIR)/bin/kickstart-mkroot
endef

$(eval $(cmake-package))
