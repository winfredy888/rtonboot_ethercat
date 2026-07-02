################################################################################
#
# ec_inter
#
################################################################################

APP_EC_INTER_VERSION = 1.0.0
APP_EC_INTER_SITE_METHOD:=local
APP_EC_INTER_SITE = $(CURDIR)/work/ec_inter
APP_EC_INTER_INSTALL_TARGET:=YES


define APP_EC_INTER_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_EC_INTER_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/ec_inter $(TARGET_DIR)/bin
endef

define APP_EC_INTER_PERMISSIONS
    /bin/ec_inter f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

