################################################################################
#
# bpak-mbedtls
#
################################################################################

# Buildroot only packages mbedtls for the target. The host bpak tool needs
# libmbedcrypto, so build the same mbedtls (3.6 LTS) for the host here.
BPAK_MBEDTLS_VERSION = $(MBEDTLS_VERSION)
BPAK_MBEDTLS_SITE = $(MBEDTLS_SITE)
BPAK_MBEDTLS_SOURCE = $(MBEDTLS_SOURCE)
BPAK_MBEDTLS_DL_SUBDIR = mbedtls
BPAK_MBEDTLS_LICENSE = Apache-2.0 or GPL-2.0+
BPAK_MBEDTLS_LICENSE_FILES = LICENSE

HOST_BPAK_MBEDTLS_CONF_OPTS = \
	-DENABLE_PROGRAMS=OFF \
	-DENABLE_TESTING=OFF \
	-DGEN_FILES=OFF \
	-DUSE_SHARED_MBEDTLS_LIBRARY=ON \
	-DUSE_STATIC_MBEDTLS_LIBRARY=OFF

$(eval $(host-cmake-package))
