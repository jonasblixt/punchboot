################################################################################
#
# bpak
#
################################################################################

BPAK_VERSION := $(call pb_testbench_git_head,$(PB_TESTBENCH_BPAK_REPO),$(PB_TESTBENCH_BPAK_BRANCH))
BPAK_SITE = $(PB_TESTBENCH_BPAK_REPO)
BPAK_SITE_METHOD = git
# No .hash file: the tarball of a branch head changes with every commit,
# Buildroot only warns about a package without one
BPAK_LICENSE = BSD-3-Clause
BPAK_LICENSE_FILES = LICENSE

# The library for the target, kickstart links it
BPAK_INSTALL_STAGING = YES
BPAK_INSTALL_TARGET = NO
BPAK_DEPENDENCIES = mbedtls xz
BPAK_CONF_OPTS = -DBPAK_BUILD_TESTS=OFF -DBPAK_WERROR=OFF

# The bpak tool for the host is the bpak python package, its C extension
# built by scikit-build-core with CMake
HOST_BPAK_SETUP_TYPE = pep517
HOST_BPAK_DEPENDENCIES = \
	$(BR2_CMAKE_HOST_DEPENDENCY) \
	host-bpak-click \
	host-bpak-mbedtls \
	host-python-scikit-build-core \
	host-xz
HOST_BPAK_ENV = CMAKE_PREFIX_PATH=$(HOST_DIR)

$(eval $(cmake-package))
$(eval $(host-python-package))
