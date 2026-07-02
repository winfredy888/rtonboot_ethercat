################################################################################
#
# motorpp
#
################################################################################

APP_MOTORPP_VERSION = 1.0.0
APP_MOTORPP_SITE_METHOD:=local
APP_MOTORPP_SITE = $(CURDIR)/work/motorpp
APP_MOTORPP_INSTALL_TARGET:=YES


define APP_MOTORPP_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_MOTORPP_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/motorpp $(TARGET_DIR)/bin
endef

define APP_MOTORPP_PERMISSIONS
    /bin/motorpp f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

