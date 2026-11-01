#!/bin/sh
# Build t150-dkms as a noarch RPM from this git tree.
# Needs rpmbuild (Fedora: dnf install rpm-build).

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
VERSION=1.1
NAME="t150-$VERSION"

if ! command -v rpmbuild >/dev/null 2>&1; then
	echo "rpmbuild not found. On Fedora: sudo dnf install rpm-build systemd-rpm-macros" >&2
	exit 1
fi

TOP=$(mktemp -d)
trap 'rm -rf "$TOP"' EXIT

mkdir -p "$TOP/$NAME/hid-t150" "$TOP/$NAME/files" "$TOP/$NAME/common"
cp -a "$ROOT/hid-t150/"*.c "$ROOT/hid-t150/"*.h "$ROOT/hid-t150/Makefile" \
	"$TOP/$NAME/hid-t150/"
cp -a "$ROOT/common/t150_presets.h" "$TOP/$NAME/common/"
cp -a "$ROOT/dkms.conf" "$ROOT/dkms_make.mak" "$ROOT/LICENSE" "$ROOT/README.md" \
	"$TOP/$NAME/"
cp -a "$ROOT/files/etc" "$ROOT/files/usr" "$TOP/$NAME/files/"

mkdir -p "$HOME/rpmbuild/SOURCES" "$HOME/rpmbuild/SPECS"
tar -C "$TOP" -czf "$HOME/rpmbuild/SOURCES/$NAME.tar.gz" "$NAME"
cp "$SCRIPT_DIR/t150-dkms.spec" "$HOME/rpmbuild/SPECS/"

echo "==== rpmbuild -ba t150-dkms.spec ===="
rpmbuild -ba "$HOME/rpmbuild/SPECS/t150-dkms.spec"

echo "RPMs under $HOME/rpmbuild/RPMS/"
find "$HOME/rpmbuild/RPMS" "$HOME/rpmbuild/SRPMS" -name "t150-dkms-$VERSION*" -print
