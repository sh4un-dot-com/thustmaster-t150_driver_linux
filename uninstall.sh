#!/bin/bash

set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
VERSION=1.1

if [ "${EUID}" -ne 0 ]; then
	echo "You are not running this script as root!"
	exit 1
fi

rmmod hid_t150 2>/dev/null || true

if command -v dkms >/dev/null 2>&1; then
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
	rm -rf /usr/src/t150-*
fi

rm -f /etc/udev/rules.d/10-t150.rules \
	/etc/udev/rules.d/99-t150-defaults.rules \
	/etc/modules-load.d/hid-t150.conf \
	/etc/modprobe.d/hid-t150.conf \
	/usr/lib/udev/hid-t150-rebind

# Current-kernel extra module from `make install` (plain or zstd-compressed)
KVER=$(uname -r)
rm -f "/lib/modules/$KVER/extra/hid-t150/hid-t150.ko" \
	"/lib/modules/$KVER/extra/hid-t150/hid-t150.ko.zst" \
	"/lib/modules/$KVER/extra/hid-t150.ko" \
	"/lib/modules/$KVER/extra/hid-t150.ko.zst" \
	"/lib/modules/$KVER/updates/dkms/hid-t150.ko" \
	"/lib/modules/$KVER/updates/dkms/hid-t150.ko.zst" \
	"/lib/modules/$KVER/kernel/drivers/input/joystick/hid-t150.ko" \
	"/lib/modules/$KVER/kernel/drivers/input/joystick/hid-t150.ko.zst"
rmdir "/lib/modules/$KVER/extra/hid-t150" 2>/dev/null || true
depmod -a "$KVER" || true

udevadm control --reload || true

echo "hid-t150 uninstalled."
