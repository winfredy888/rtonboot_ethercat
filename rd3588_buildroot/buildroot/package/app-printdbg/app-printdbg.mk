################################################################################
#
# printdbg
#
################################################################################

APP_PRINTDBG_VERSION = 1.0.0
APP_PRINTDBG_SITE_METHOD:=local
APP_PRINTDBG_SITE = $(CURDIR)/work/printdbg
APP_PRINTDBG_INSTALL_TARGET:=YES


define APP_PRINTDBG_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_PRINTDBG_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/printdbg $(TARGET_DIR)/bin
endef

define APP_PRINTDBG_PERMISSIONS
    /bin/printdbg f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

