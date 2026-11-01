#!/bin/sh
# Install ht150.ko, loader.conf snippet, and optional ht150ctl.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

if [ "$(uname -s)" != "FreeBSD" ]; then
	echo "This script is for FreeBSD. On Linux use ../install.sh" >&2
	exit 1
fi

if [ "$(id -u)" -ne 0 ]; then
	echo "Run as root." >&2
	exit 1
fi

if [ ! -f /usr/src/sys/conf/kmod.mk ] && [ -z "${SYSDIR:-}" ]; then
	echo "FreeBSD kernel source is required to build ht150.ko." >&2
	echo "  pkg install git" >&2
	echo "  git clone --depth 1 https://git.freebsd.org/src.git /usr/src" >&2
	echo "Or set SYSDIR to an existing sys tree." >&2
	exit 1
fi

echo "==== BUILDING ht150.ko ===="
make -C "$SCRIPT_DIR"

echo "==== INSTALLING /boot/modules/ht150.ko ===="
make -C "$SCRIPT_DIR" install

if [ -f "$SCRIPT_DIR/ht150ctl.c" ]; then
	echo "==== BUILDING ht150ctl ===="
	cc -O2 -pipe -o "$SCRIPT_DIR/ht150ctl" "$SCRIPT_DIR/ht150ctl.c"
	install -m 755 "$SCRIPT_DIR/ht150ctl" /usr/local/sbin/ht150ctl
fi

install -d /usr/local/etc/devd
install -m 644 "$ROOT_DIR/files/freebsd/devd/ht150.conf" \
	/usr/local/etc/devd/ht150.conf
service devd restart >/dev/null 2>&1 || true

LOADER=/boot/loader.conf
touch "$LOADER"
if ! grep -q '^hw.usb.usbhid.enable=' "$LOADER" 2>/dev/null; then
	printf '\n# T150/TMX: use usbhid(4) so hidbus can attach ht150\nhw.usb.usbhid.enable=1\n' >> "$LOADER"
	echo "Appended hw.usb.usbhid.enable=1 to $LOADER (reboot to apply)."
fi

if command -v sysrc >/dev/null 2>&1; then
	sysrc kld_list+=" ht150" >/dev/null
else
	if ! grep -q 'ht150_load=' "$LOADER" 2>/dev/null; then
		printf 'ht150_load="YES"\n' >> "$LOADER"
	fi
fi

kldload ht150 2>/dev/null || true

echo "Done. With usbhid enabled and the T150 in PS3 mode:"
echo "  kldstat | grep ht150"
echo "  sysctl -a | grep ht150"
echo "  dmesg | grep -i ht150"
echo "  ht150ctl"
echo "A reboot is needed if usbhid was not already enabled."
