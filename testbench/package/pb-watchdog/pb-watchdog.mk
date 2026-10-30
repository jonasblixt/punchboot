################################################################################
#
# pb-watchdog
#
################################################################################

PB_WATCHDOG_VERSION = 1.0
PB_WATCHDOG_SITE = $(BR2_EXTERNAL_PB_TESTBENCH_PATH)/package/pb-watchdog/src
PB_WATCHDOG_SITE_METHOD = local
PB_WATCHDOG_LICENSE = BSD-3-Clause

define PB_WATCHDOG_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) $(TARGET_LDFLAGS) -Wall -Wextra \
		-o $(@D)/pb-watchdog $(@D)/pb-watchdog.c
endef

define PB_WATCHDOG_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/pb-watchdog $(TARGET_DIR)/usr/sbin/pb-watchdog
endef

define PB_WATCHDOG_INSTALL_INIT_SYSV
	$(INSTALL) -D -m 0755 $(BR2_EXTERNAL_PB_TESTBENCH_PATH)/package/pb-watchdog/S15watchdog \
		$(TARGET_DIR)/etc/init.d/S15watchdog
endef

$(eval $(generic-package))
