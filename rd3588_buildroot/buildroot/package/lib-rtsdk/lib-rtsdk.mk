LIB_RTSDK_VERSION = 1.0
LIB_RTSDK_SITE_METHOD = local
LIB_RTSDK_SITE = $(CURDIR)/work/librtsdk
LIB_RTSDK_INSTALL_TARGET:=YES

define LIB_RTSDK_BUILD_CMDS
    $(TARGET_MAKE_ENV) $(MAKE) -C $(@D)
endef

define LIB_RTSDK_INSTALL_TARGET_CMDS
    $(INSTALL) -m 0644 -D $(@D)/librtsdk.so $(TARGET_DIR)/usr/lib/librtsdk.so
    # cp -dpf $(@D)/librtsdk.a $(TARGET_DIR)/lib/
    # $(INSTALL) -D -m 0644 $(@D)/*.h $(TARGET_DIR)/usr/include
endef

define LIB_RTSDK_PERMISSIONS
    /usr/lib/librtsdk.so f 4644 0 0 - - - - -
endef

$(eval $(cmake-package))
