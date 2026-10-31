obj-m += hid-t150.o
KDIR ?= /lib/modules/$(shell uname -r)/build
KBUILD_ARGS := $(shell grep -q '^CONFIG_CC_IS_CLANG=y' $(KDIR)/include/config/auto.conf 2>/dev/null && printf '%s' 'LLVM=1')
INSTALL_MOD_DIR ?= extra/hid-t150

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) clean

install:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $(KBUILD_ARGS) INSTALL_MOD_DIR=$(INSTALL_MOD_DIR) modules_install
