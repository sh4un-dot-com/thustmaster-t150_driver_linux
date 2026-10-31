# Thrustmaster T150 and TMX Force Feedback Wheel Linux driver

**Disclaimer**
This is not an official driver from Thrustmaster and is provided without any
kind of warranty. Loading and using this driver is at your own risk. The
authors take no responsibility for kernel panics, bricked devices, or any
other inconvenience.

Out-of-tree HID driver (`hid-t150`) for:

| Wheel | USB ID after init | Rotation | Notes |
|-------|-------------------|----------|--------|
| T150 / T150RS | `044f:b677` | 270°–1080° | Mode switch on **PS3** before plugging in |
| TMX | `044f:b67f` | 270°–900° | Needs a separate init driver first |

The in-tree `hid-thrustmaster` driver (kernel 5.13+) initializes a T150 from
boot ID `044f:b65d` into full T150RS mode. TMX init is still
[TMX-Driver](https://github.com/emtek995/TMX-driver) or
[tmdrv](https://github.com/her001/tmdrv).

## Status

Verified on CachyOS Linux 7.3 with a T150RS (firmware 35) in PS3 mode.

### Working

- All wheel and pedal axes, and buttons
- Axis fuzz/flat cleared (the HID descriptor otherwise adds a fake center deadzone)
- Force feedback, uploaded from a workqueue (safe from input IRQ context)
  - `FF_GAIN`, `FF_AUTOCENTER`
  - `FF_CONSTANT` (with attack/fade envelopes)
  - `FF_SINE`, `FF_SAW_UP`, `FF_SAW_DOWN`
  - `FF_SPRING`, `FF_DAMPER` (scaled by `spring_level` / `damper_level`)
  - `FF_RUMBLE` rewritten as a short east-facing sine
- Up to **9** simultaneous effects (hardware packet IDs are 8-bit)
- Sysfs knobs for range, gain, autocenter, spring/damper levels
- [Oversteer](https://github.com/berarma/oversteer) compatible
- Firmware version reported (vendor GET request 86)
- `hid-generic` is rebound off the wheel so FFB is actually available

### Missing

- Device-side firmware upgrades (`firmware_upgrade` returns `-EOPNOTSUPP`)
- `FF_FRICTION` and `FF_INERTIA` until the USB commit codes are confirmed

The wheel's vendor GET often returns a short packet (firmware in byte 1,
not a full 8-byte settings block). Probe still applies default gain (~75%),
full range, and autocenter off. After that, sysfs reads use the driver's
cache. Writes always go out on the interrupt OUT endpoint.

## Install

Needs matching kernel headers (`linux-headers` / `linux-cachyos-headers`)
and a compiler. Clang-built kernels (CachyOS and some custom Arch/Fedora
kernels) are detected automatically; the Makefile passes `LLVM=1`.

```sh
sudo ./install.sh
```

The script:

1. Builds the module (DKMS when `dkms` is installed, otherwise for the
   running kernel only)
2. Installs udev rules, the hid-generic rebind helper, `modules-load.d`,
   and `modprobe.d`
3. Loads `hid-t150` and rebinds any already-plugged T150/TMX

`hid-generic` matches every HID device, and these IDs are not on the
kernel special-driver list, so a T150/TMX otherwise shows up as a joystick
**without force feedback**. The udev helper
`/usr/lib/udev/hid-t150-rebind` steals the device on bind.

Install [DKMS](https://github.com/dkms-project/dkms) if you want the
module rebuilt on kernel upgrades:

```sh
# Arch / CachyOS
sudo pacman -S dkms
sudo ./install.sh
```

Without DKMS, run `sudo ./install.sh` again after a kernel update.

Unsigned out-of-tree modules load when signature enforcement is off
(`module.sig_enforce=0`, the CachyOS default). `dmesg` may report
`module verification failed: signature and/or required key missing` and
taint the kernel; that is expected.

### Check that the driver owns the wheel

```sh
lsmod | grep hid_t150
ls /sys/bus/hid/drivers/hid-t150
dmesg | grep -i t150
./hid-t150/test_sysfs.sh
```

You should see a hid device such as `0003:044F:B677.xxxx` under the
`hid-t150` driver, firmware_version readable, and `capabilities/ff`
non-zero on the input device.

Force feedback: `fftest /dev/input/eventN` from linuxconsole tools, or
any game that uses Linux FF.

Uninstall:

```sh
sudo ./uninstall.sh
```

### Manual build

```sh
make
sudo insmod hid-t150/hid-t150.ko
sudo /usr/lib/udev/hid-t150-rebind 0003:044F:B677.xxxx   # after installing the helper
```

`insmod` alone leaves an already-probed wheel on `hid-generic`. Prefer
`install.sh`, or run the rebind helper with the hid kernel name from
`ls /sys/bus/hid/devices`.

## Sysfs

Attributes live on the **HID** device, not the input node:

```
/sys/bus/hid/devices/0003:044F:B677.*/
/sys/bus/hid/devices/0003:044F:B67F.*/
```

`dmesg` prints the hid path, for example
`hid-t150 0003:044F:B677.0028: T150/TMX wheel ready`.

udev chmods the writable knobs to `0666` so Oversteer can use them as a
normal user. (The kernel forbids world-writable `DEVICE_ATTR` modes, so
the driver creates them as `0664` and udev opens them up.)

| Attribute | Values | Description |
|-----------|--------|-------------|
| `range` | 270–1080 (TMX max 900) | Wheel rotation in degrees |
| `gain` | 0–65535 | Global FFB intensity |
| `autocenter` | 0–65535 | Hardware spring-back strength |
| `enable_autocenter` | `y` / `n` | Keep hardware autocenter while a game has the device open |
| `spring_level` | 0–100 | Scale applied to `FF_SPRING` |
| `damper_level` | 0–100 | Scale applied to `FF_DAMPER` |
| `firmware_version` | integer, read-only | Firmware reported by the wheel |
| `max_range` | 1080 or 900, read-only | Hardware maximum rotation |
| `settings_cache_age_ms` | integer or `-1` | Age of the cached settings |
| `supported_ff_effects` | tokens, read-only | Effects this driver exposes |
| `reload_settings` | write-only, root | Force a vendor GET (short reads still succeed) |
| `firmware_upgrade` | write-only | Always `-EOPNOTSUPP` |

Reads use a 300 ms cache so Oversteer polling does not hammer USB.
Writes go to the wheel immediately and update the cache.

Oversteer drives `range`, `gain`, `autocenter`, `spring_level`, and
`damper_level`.

### Custom plug-in defaults

`install.sh` copies a commented template to
`/etc/udev/rules.d/99-t150-defaults.rules`. Uncomment and edit:

```
SUBSYSTEM=="hid", DRIVER=="hid-t150", ATTR{range}="900", ATTR{gain}="49150"
```

Then:

```sh
sudo udevadm control --reload
sudo udevadm trigger
```

## Troubleshooting

**Joystick works, FFB does not.** The device is still on `hid-generic`.

```sh
ls -l /sys/bus/hid/devices/0003:044F:B677.*/driver
sudo /usr/lib/udev/hid-t150-rebind "$(basename /sys/bus/hid/devices/0003:044F:B677.*)"
```

The driver symlink should end in `hid-t150`.

**Wheel never reaches `044f:b677`.** T150: set the switch to PS3, unplug,
replug. Confirm `hid-thrustmaster` initialized it (`dmesg | grep -i thrust`).
TMX: load TMX-Driver or tmdrv first.

**Module missing after a reboot or kernel update.**
`lsmod | grep hid_t150` empty: `sudo ./install.sh` again. Install `dkms`
to survive kernel upgrades.

**`sign-file: certs/signing_key.pem` during install.** The distro kernel
is signed; headers do not ship a private key. Harmless when
`module.sig_enforce` is `N`.

## License

GPL-2.0. See [LICENSE](LICENSE).
