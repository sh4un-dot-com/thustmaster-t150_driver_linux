#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ]; then
	echo "Run as root." >&2
	exit 1
fi

kldunload ht150 2>/dev/null || true

rm -f /boot/modules/ht150.ko \
	/usr/local/sbin/ht150ctl \
	/usr/local/etc/devd/ht150.conf

# Subtract ht150 only. `sysrc -x kld_list` would wipe every other module.
if command -v sysrc >/dev/null 2>&1; then
	sysrc kld_list-=" ht150" >/dev/null 2>&1 || true
	sysrc kld_list-="ht150" >/dev/null 2>&1 || true
fi

echo "ht150 uninstalled. Leave hw.usb.usbhid.enable in /boot/loader.conf if other HID devices still need usbhid(4)."
