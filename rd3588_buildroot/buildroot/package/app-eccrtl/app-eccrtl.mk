################################################################################
#
# eccrtl
#
################################################################################

APP_ECCRTL_VERSION = 1.0.0
APP_ECCRTL_SITE_METHOD:=local
APP_ECCRTL_SITE = $(CURDIR)/work/eccrtl
APP_ECCRTL_INSTALL_TARGET:=YES


define APP_ECCRTL_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_ECCRTL_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/eccrtl $(TARGET_DIR)/bin
endef

define APP_ECCRTL_INSTALL_INIT_SYSV
	$(INSTALL) -m 0755 -D $(@D)/S99_eccrtl  \
		$(TARGET_DIR)/etc/init.d/S99_eccrtl
endef


define APP_ECCRTL_PERMISSIONS
    /bin/eccrtl f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

