################################################################################
#
# ec_cyclic
#
################################################################################

APP_EC_CYCLIC_VERSION = 1.0.0
APP_EC_CYCLIC_SITE_METHOD:=local
APP_EC_CYCLIC_SITE = $(CURDIR)/work/ec_cyclic
APP_EC_CYCLIC_INSTALL_TARGET:=YES


define APP_EC_CYCLIC_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_EC_CYCLIC_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/ec_cyclic $(TARGET_DIR)/bin
endef

define APP_EC_CYCLIC_PERMISSIONS
    /bin/ec_cyclic f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

