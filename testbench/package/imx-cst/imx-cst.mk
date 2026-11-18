################################################################################
#
# imx-cst
#
################################################################################

# NXP only publishes CST behind a login, Debian ships the sources.
IMX_CST_VERSION = 3.4.1
IMX_CST_SOURCE = imx-code-signing-tool_$(IMX_CST_VERSION)+dfsg.orig.tar.xz
IMX_CST_SITE = https://deb.debian.org/debian/pool/main/i/imx-code-signing-tool
IMX_CST_LICENSE = BSD-3-Clause
IMX_CST_LICENSE_FILES = LICENSE.bsd3

HOST_IMX_CST_DEPENDENCIES = host-bison host-flex host-openssl

# binaries.mk is used directly: obj.linux64/Makefile adds -m64.
HOST_IMX_CST_MAKE_OPTS = \
	-C $(@D)/code/obj.linux64 -f ../build/make/binaries.mk \
	OSTYPE=linux64 \
	TOOLCHAIN=buildroot \
	ENCRYPTION=yes \
	VERSION=$(IMX_CST_VERSION) \
	_OPENSSL_PATH=$(HOST_DIR) \
	CC="$(HOSTCC)" \
	AR="$(HOSTAR)" \
	YACC="$(HOST_DIR)/bin/bison -y" \
	LEX="$(HOST_DIR)/bin/flex" \
	BR_CFLAGS="$(HOST_CFLAGS)" \
	BR_LDFLAGS="$(HOST_LDFLAGS)"

define HOST_IMX_CST_BUILD_CMDS
	$(INSTALL) -m 0644 $(HOST_IMX_CST_PKGDIR)/buildroot.mk \
		$(@D)/code/build/make/buildroot.mk
	$(HOST_MAKE_ENV) $(MAKE) $(HOST_IMX_CST_MAKE_OPTS) build
endef

define HOST_IMX_CST_INSTALL_CMDS
	$(INSTALL) -D -m 0755 $(@D)/code/obj.linux64/cst $(HOST_DIR)/bin/cst
	$(INSTALL) -D -m 0755 $(@D)/code/obj.linux64/srktool $(HOST_DIR)/bin/srktool
endef

$(eval $(host-generic-package))
