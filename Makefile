# Linux GNU make. On FreeBSD use: make -C freebsd
# or: sudo ./install.sh

KDIR ?= /lib/modules/$(shell uname -r)/build
UNAME_S := $(shell uname -s)

.PHONY: all clean install uninstall test-encode

ifeq ($(UNAME_S),FreeBSD)
all:
	$(MAKE) -C freebsd all

clean:
	$(MAKE) -C freebsd clean

install:
	$(MAKE) -C freebsd install
else
all:
	$(MAKE) -C hid-t150 KDIR="$(KDIR)" all

clean:
	$(MAKE) -C hid-t150 KDIR="$(KDIR)" clean

install:
	$(MAKE) -C hid-t150 KDIR="$(KDIR)" install
endif

uninstall:
	@echo "Run ./uninstall.sh as root to remove DKMS/udev files as well."

test-encode:
	$(CC) -O2 -Wall -Wextra -Icommon -o /tmp/t150_test_encode common/test_encode.c
	/tmp/t150_test_encode
	rm -f /tmp/t150_test_encode
