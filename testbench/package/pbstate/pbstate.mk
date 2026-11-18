################################################################################
#
# pbstate
#
################################################################################

PBSTATE_VERSION = $(shell cat $(PB_TESTBENCH_PUNCHBOOT_SRCDIR)/version.txt)
PBSTATE_SITE = $(PB_TESTBENCH_PUNCHBOOT_SRCDIR)
PBSTATE_SITE_METHOD = local
PBSTATE_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = $(PB_TESTBENCH_PUNCHBOOT_RSYNC_EXCLUSIONS)
PBSTATE_SUBDIR = tools/pbstate
PBSTATE_LICENSE = BSD-3-Clause
PBSTATE_LICENSE_FILES = LICENSE
PBSTATE_INSTALL_STAGING = YES

$(eval $(cmake-package))
