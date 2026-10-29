################################################################################
#
# bpak-click
#
################################################################################

# Buildroot only packages python-click for the target. The host bpak tool
# needs it, so build the same click for the host here.
BPAK_CLICK_VERSION = $(PYTHON_CLICK_VERSION)
BPAK_CLICK_SITE = $(PYTHON_CLICK_SITE)
BPAK_CLICK_SOURCE = $(PYTHON_CLICK_SOURCE)
BPAK_CLICK_DL_SUBDIR = python-click
BPAK_CLICK_SETUP_TYPE = $(PYTHON_CLICK_SETUP_TYPE)
BPAK_CLICK_LICENSE = $(PYTHON_CLICK_LICENSE)
BPAK_CLICK_LICENSE_FILES = $(PYTHON_CLICK_LICENSE_FILES)

$(eval $(host-python-package))
