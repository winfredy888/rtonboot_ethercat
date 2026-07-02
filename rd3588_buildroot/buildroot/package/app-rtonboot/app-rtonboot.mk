################################################################################
#
# rtonboot
#
################################################################################

APP_RTONBOOT_VERSION = 1.0.0
APP_RTONBOOT_SITE_METHOD:=local
APP_RTONBOOT_SITE = $(CURDIR)/work/rtonboot
APP_RTONBOOT_INSTALL_TARGET:=YES


define APP_RTONBOOT_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_RTONBOOT_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/rtonboot $(TARGET_DIR)/bin
endef

define APP_RTONBOOT_PERMISSIONS
    /bin/rtonboot f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

