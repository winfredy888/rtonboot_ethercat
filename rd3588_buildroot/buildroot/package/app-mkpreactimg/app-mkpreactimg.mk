################################################################################
#
# mkpreactimg
#
################################################################################

APP_MKPREACTIMG_VERSION = 1.0.0
APP_MKPREACTIMG_SITE_METHOD:=local
APP_MKPREACTIMG_SITE = $(CURDIR)/work/mkpreactimg
APP_MKPREACTIMG_INSTALL_TARGET:=YES

JIHUOMA_HEADER_FILE = $(APP_MKPREACTIMG_SITE)/jihuoma.h

define get_enable_jihuoma
$(shell \
    if [ -f "$(JIHUOMA_HEADER_FILE)" ]; then \
        VAL=$$(grep -E '^#define[[:space:]]+ENABLE_JIHUOMA[[:space:]]+[0-9]+' "$(JIHUOMA_HEADER_FILE)" 2>/dev/null | tail -1 | sed -E 's/^#define[[:space:]]+ENABLE_JIHUOMA[[:space:]]+([0-9]+)/\1/'); \
        if [ -z "$$VAL" ] || [ "$$VAL" = "0" ]; then \
            echo 0; \
        else \
            echo "$$VAL"; \
        fi; \
    else \
        echo 0; \
    fi \
)
endef

CONFIG_ENABLE_JIHUOMA := $(call get_enable_jihuoma)

$(info ENABLE_JIHUOMA value: $(CONFIG_ENABLE_JIHUOMA))

ifneq ($(CONFIG_ENABLE_JIHUOMA),0)

define APP_MKPREACTIMG_BUILD_CMDS
    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D) all
endef

define APP_MKPREACTIMG_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/mkpreactimg $(TARGET_DIR)/bin
endef

define APP_MKPREACTIMG_PERMISSIONS
    /bin/mkpreactimg f 4755 0 0 - - - - -
endef

$(eval $(cmake-package))

else
$(eval $(virtual-package))
endif
