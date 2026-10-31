#!/bin/bash

set -euo pipefail

VERSION=1.1
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REBIND_HELPER=/usr/lib/udev/hid-t150-rebind

cd "$SCRIPT_DIR"

if [ "${EUID}" -ne 0 ]; then
	echo "You are not running this script as root!"
	exit 1
fi

echo "==== UNLOADING OLD MODULE ===="
rmmod hid_t150 2>/dev/null || true

install_udev_and_autoload() {
	echo "==== INSTALLING UDEV RULES AND AUTOLOAD ===="
	install -d /etc/udev/rules.d /etc/modules-load.d /etc/modprobe.d /usr/lib/udev
	install -m 644 "$SCRIPT_DIR/files/etc/udev/rules.d/10-t150.rules" /etc/udev/rules.d/
	install -m 644 "$SCRIPT_DIR/files/etc/udev/rules.d/99-t150-defaults.rules" /etc/udev/rules.d/
	install -m 644 "$SCRIPT_DIR/files/etc/modules-load.d/hid-t150.conf" /etc/modules-load.d/
	install -m 644 "$SCRIPT_DIR/files/etc/modprobe.d/hid-t150.conf" /etc/modprobe.d/
	install -m 755 "$SCRIPT_DIR/files/usr/lib/udev/hid-t150-rebind" "$REBIND_HELPER"
	udevadm control --reload
}

rebind_wheels() {
	echo "==== BINDING T150/TMX TO hid-t150 ===="
	shopt -s nullglob
	local dev name
	for dev in /sys/bus/hid/devices/0003:044F:B677.* /sys/bus/hid/devices/0003:044F:B67F.*; do
		name=$(basename "$dev")
		echo "rebind $name"
		"$REBIND_HELPER" "$name" || echo "warning: could not rebind $name" >&2
	done
	shopt -u nullglob
}

if command -v dkms >/dev/null 2>&1; then
	echo "==== REMOVING OLD DKMS VERSIONS ===="
	old_vers=$(dkms status 2>/dev/null | awk -F', ' '/^t150\// {split($1, parts, "/"); print parts[2]}' || true)
	if [ -n "$old_vers" ]; then
		IFS=$'\n'
		for version in $old_vers
		do
			[ -n "$version" ] || continue
			dkms remove -m t150 -v "$version" --all || true
		done
		unset IFS
	fi

	echo "==== CONFIG DKMS ===="
	rm -rf "/usr/src/t150-$VERSION"
	install -d "/usr/src/t150-$VERSION/hid-t150" "/usr/src/t150-$VERSION/build"

	find hid-t150 -maxdepth 1 -type f \( -name '*.c' -o -name '*.h' -o -name 'Makefile' \) \
		-exec cp -a {} "/usr/src/t150-$VERSION/hid-t150/" \;
	cp ./dkms_make.mak "/usr/src/t150-$VERSION/Makefile"
	cp ./dkms.conf "/usr/src/t150-$VERSION/"

	echo "==== DKMS ===="
	dkms add -m t150 -v $VERSION --verbose
	dkms build -m t150 -v $VERSION --verbose
	dkms install -m t150 -v $VERSION --verbose
else
	echo "==== DKMS NOT FOUND, INSTALLING FOR CURRENT KERNEL ===="
	make -C "$SCRIPT_DIR/hid-t150"
	make -C "$SCRIPT_DIR/hid-t150" install
	depmod -a
fi

install_udev_and_autoload

echo "==== LOADING hid-t150 ===="
modprobe hid_t150
rebind_wheels
shopt -s nullglob
for dev in /sys/bus/hid/devices/0003:044F:B677.* /sys/bus/hid/devices/0003:044F:B67F.*; do
	udevadm trigger --action=bind "$dev" || true
	udevadm trigger --action=add "$dev" || true
done
shopt -u nullglob

echo "Done. With the T150 in PS3 mode (or a TMX after init) check:"
echo "  lsmod | grep hid_t150"
echo "  ls -l /sys/bus/hid/drivers/hid-t150"
echo "  dmesg | grep -i t150"
echo "  ./hid-t150/test_sysfs.sh"
