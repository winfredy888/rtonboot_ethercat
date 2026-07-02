################################################################################
#
# testbc
#
################################################################################

APP_TESTBC_VERSION = 1.0.0
APP_TESTBC_SITE_METHOD:=local
APP_TESTBC_SITE = $(CURDIR)/work/testbc
APP_TESTBC_INSTALL_TARGET:=YES


define APP_TESTBC_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_TESTBC_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/testbc $(TARGET_DIR)/bin
endef

define APP_TESTBC_PERMISSIONS
    /bin/testbc f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

