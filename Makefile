KDIR ?= /lib/modules/$(shell uname -r)/build

.PHONY: all clean install uninstall

all:
	$(MAKE) -C hid-t150 KDIR="$(KDIR)" all

clean:
	$(MAKE) -C hid-t150 KDIR="$(KDIR)" clean

install:
	$(MAKE) -C hid-t150 KDIR="$(KDIR)" install

uninstall:
	@echo "Run ./uninstall.sh as root to remove DKMS/udev files as well."
