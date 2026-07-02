################################################################################
#
# motorctl
#
################################################################################

APP_MOTORCTL_VERSION = 1.0.0
APP_MOTORCTL_SITE_METHOD:=local
APP_MOTORCTL_SITE = $(CURDIR)/work/motorctl
APP_MOTORCTL_INSTALL_TARGET:=YES


define APP_MOTORCTL_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_MOTORCTL_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/motorctl $(TARGET_DIR)/bin
endef

define APP_MOTORCTL_PERMISSIONS
    /bin/motorctl f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

