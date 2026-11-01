#!/bin/sh
# Install hid-t150 on Linux, or ht150 on FreeBSD.

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

os=$(uname -s)
case "$os" in
FreeBSD)
	exec "$SCRIPT_DIR/freebsd/install.sh" "$@"
	;;
Linux)
	;;
*)
	echo "Unsupported OS: $os (Linux and FreeBSD only)" >&2
	exit 1
	;;
esac

# Linux path uses bash (shopt, pipefail).
if [ -z "${BASH_VERSION:-}" ]; then
	if command -v bash >/dev/null 2>&1; then
		exec bash "$SCRIPT_DIR/install.sh" "$@"
	fi
	echo "bash is required on Linux." >&2
	exit 1
fi

set -euo pipefail

VERSION=1.1
REBIND_HELPER=/usr/lib/udev/hid-t150-rebind

cd "$SCRIPT_DIR"

if [ "${EUID}" -ne 0 ]; then
	echo "You are not running this script as root!"
	exit 1
fi

linux_id() {
	if [ -r /etc/os-release ]; then
		# shellcheck disable=SC1091
		. /etc/os-release
		printf '%s %s' "${ID:-}" "${ID_LIKE:-}"
	fi
}

print_linux_deps() {
	local ids
	ids=$(linux_id)
	echo "Kernel headers for $(uname -r) are missing ($1)."
	case " $ids " in
	*" fedora "*|*" rhel "*|*" centos "*|*" rocky "*|*" almalinux "*|*" nobara "*)
		echo "On Fedora / RHEL-like:"
		echo "  sudo dnf install kernel-devel-\$(uname -r) gcc make elfutils-libelf-devel"
		echo "Optional, rebuilds the module on kernel upgrades:"
		echo "  sudo dnf install dkms"
		;;
	*" arch "*|*" cachyos "*|*" manjaro "*|*" endeavouros "*)
		echo "On Arch / CachyOS:"
		echo "  sudo pacman -S linux-headers gcc make   # or linux-cachyos-headers"
		echo "Optional: sudo pacman -S dkms"
		;;
	*" debian "*|*" ubuntu "*|*" linuxmint "*)
		echo "On Debian / Ubuntu:"
		echo "  sudo apt install linux-headers-\$(uname -r) gcc make"
		echo "Optional: sudo apt install dkms"
		;;
	*)
		echo "Install matching kernel headers, gcc, and make, then rerun."
		;;
	esac
}

selinux_relabel() {
	if command -v restorecon >/dev/null 2>&1; then
		echo "==== SELinux restorecon ===="
		restorecon -F "$REBIND_HELPER" \
			/usr/bin/t150-preset \
			/etc/udev/rules.d/10-t150.rules \
			/etc/udev/rules.d/99-t150-defaults.rules \
			/etc/modules-load.d/hid-t150.conf \
			/etc/modprobe.d/hid-t150.conf \
			2>/dev/null || true
	fi
}

secure_boot_note() {
	if command -v mokutil >/dev/null 2>&1 && \
		mokutil --sb-state 2>/dev/null | grep -qi 'SecureBoot enabled'; then
		echo "==== Secure Boot is enabled ===="
		echo "The module must be signed with a Machine Owner Key or it will not load."
		if [ -f /var/lib/dkms/mok.pub ]; then
			echo "  sudo mokutil --import /var/lib/dkms/mok.pub"
			echo "then reboot and finish enrollment in the MOK manager."
		elif [ -f /etc/pki/akmods/certs/public_key.der ]; then
			echo "  sudo mokutil --import /etc/pki/akmods/certs/public_key.der"
			echo "then reboot and finish enrollment in the MOK manager."
		else
			echo "Install dkms (or akmods) so a MOK is generated, then:"
			echo "  sudo mokutil --import /var/lib/dkms/mok.pub"
		fi
	fi
}

if [ -e /run/ostree-booted ]; then
	echo "Warning: ostree system (Silverblue / Kinoite / CoreOS)."
	echo "DKMS and writing under /usr are poorly supported here."
	echo "Prefer Fedora Workstation, a VM, or an rpm-ostree layered RPM."
fi

KVER=$(uname -r)
KBUILD=/lib/modules/$KVER/build
if [ ! -d "$KBUILD" ]; then
	print_linux_deps "$KBUILD"
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
	install -m 755 "$SCRIPT_DIR/files/usr/bin/t150-preset" /usr/bin/t150-preset
	selinux_relabel
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
	install -d "/usr/src/t150-$VERSION/hid-t150" "/usr/src/t150-$VERSION/build" \
		"/usr/src/t150-$VERSION/common"

	find hid-t150 -maxdepth 1 -type f \( -name '*.c' -o -name '*.h' -o -name 'Makefile' \) \
		-exec cp -a {} "/usr/src/t150-$VERSION/hid-t150/" \;
	cp ./common/t150_presets.h "/usr/src/t150-$VERSION/common/"
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

secure_boot_note

echo "Done. With the T150 in PS3 mode (or a TMX after init) check:"
echo "  lsmod | grep hid_t150"
echo "  ls -l /sys/bus/hid/drivers/hid-t150"
echo "  dmesg | grep -i t150"
echo "  ./hid-t150/test_sysfs.sh"
echo "  t150-preset            # show current feel preset (default: sim)"
echo "  t150-preset list       # sim arcade street offroad rally drift truck"
echo "  t150-preset drift      # switch wheel feel"
