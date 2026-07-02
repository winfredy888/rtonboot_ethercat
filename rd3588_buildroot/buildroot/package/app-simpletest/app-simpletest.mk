################################################################################
#
# simpletest
#
################################################################################

APP_SIMPLETEST_VERSION = 1.0.0
APP_SIMPLETEST_SITE_METHOD:=local
APP_SIMPLETEST_SITE = $(CURDIR)/work/simpletest
APP_SIMPLETEST_INSTALL_TARGET:=YES


define APP_SIMPLETEST_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_SIMPLETEST_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/simpletest $(TARGET_DIR)/bin
endef

define APP_SIMPLETEST_PERMISSIONS
    /bin/simpletest f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

