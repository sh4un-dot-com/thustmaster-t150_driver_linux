#!/usr/bin/env bash
set -euo pipefail

# Print hid-t150 sysfs knobs. Optional write test of gain when run as root.

devdir=""
for d in /sys/bus/hid/drivers/hid-t150/0003:*; do
	[ -e "$d/firmware_version" ] || continue
	devdir=$(readlink -f "$d")
	break
done

if [ -z "$devdir" ]; then
	echo "No hid-t150 device found. Is the module loaded and the wheel plugged in?"
	echo "  lsmod | grep hid_t150"
	echo "  ls /sys/bus/hid/drivers/hid-t150"
	echo "  dmesg | grep -i t150"
	exit 1
fi

echo "Device dir: $devdir"
echo "Driver: $(basename "$(readlink "$devdir/driver" 2>/dev/null || echo none)")"

for name in firmware_version max_range range autocenter enable_autocenter gain spring_level damper_level supported_ff_effects settings_cache_age_ms; do
	if [ -e "$devdir/$name" ]; then
		echo "$name: $(cat "$devdir/$name" 2>/dev/null || echo unreadable)"
	fi
done

echo
if [ -w "$devdir/reload_settings" ]; then
	echo "Refreshing settings (writing to reload_settings)..."
	echo 1 > "$devdir/reload_settings"
	echo "Reload written"
else
	echo "reload_settings is not writable (root-only); skipping hardware re-read."
fi

echo "Updated values:"
cat "$devdir/firmware_version" || true
cat "$devdir/range" || true
cat "$devdir/gain" || true

if [ "$(id -u)" -ne 0 ]; then
	echo "Not running as root; skipping write tests (gain change)."
	exit 0
fi

echo "Setting gain to 32768..."
printf "%d\n" 32768 > "$devdir/gain"
echo "Gain now: $(cat "$devdir/gain")"

echo "Done."
