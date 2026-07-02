################################################################################
#
# rtethcrtl
#
################################################################################

APP_RTETHCRTL_VERSION = 1.0.0
APP_RTETHCRTL_SITE_METHOD:=local
APP_RTETHCRTL_SITE = $(CURDIR)/work/rtethcrtl
APP_RTETHCRTL_INSTALL_TARGET:=YES

define APP_RTETHCRTL_INSTALL_INIT_SYSV
	$(INSTALL) -m 0755 -D $(@D)/S98_rtethcrtl  \
		$(TARGET_DIR)/etc/init.d/S98_rtethcrtl
endef

$(eval $(generic-package))

