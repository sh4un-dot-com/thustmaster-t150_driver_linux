# Thrustmaster T150 and TMX Force Feedback Wheel driver

**Disclaimer**
This is not an official driver from Thrustmaster and is provided without any
kind of warranty. Loading and using this driver is at your own risk. The
authors take no responsibility for kernel panics, bricked devices, or any
other inconvenience.

Out-of-tree HID driver: `hid-t150` on Linux, `ht150` on FreeBSD, for:

| Wheel | USB ID after init | Rotation | Notes |
|-------|-------------------|----------|--------|
| T150 / T150RS | `044f:b677` | 270°–1080° | Mode switch on **PS3** before plugging in |
| TMX | `044f:b67f` | 270°–900° | Linux: TMX-Driver or tmdrv first. FreeBSD: `ht150init` in `ht150.ko` |

The in-tree `hid-thrustmaster` driver (kernel 5.13+) initializes a T150 from
boot ID `044f:b65d` into full T150RS mode. TMX init is still
[TMX-Driver](https://github.com/emtek995/TMX-driver) or
[tmdrv](https://github.com/her001/tmdrv).

## Status

Verified on CachyOS Linux 7.3 with a T150RS (firmware 35) in PS3 mode.

Fedora/RHEL-like packaging and install support is in this tree; it was not
hardware-tested on Fedora here. FreeBSD has a hidbus module in `freebsd/`;
it was not hardware-tested on FreeBSD here.

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

Needs matching kernel headers and a compiler. Clang-built kernels (CachyOS
and some custom Arch/Fedora kernels) are detected automatically; the
Makefile passes `LLVM=1`. Stock Fedora uses GCC and does not need that.

```sh
sudo ./install.sh
```

On FreeBSD this runs `freebsd/install.sh`. On Linux it builds `hid-t150`.

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

# Fedora / RHEL-like
sudo dnf install dkms kernel-devel-$(uname -r) gcc make elfutils-libelf-devel
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

### Fedora / RHEL / CentOS Stream / Rocky / Alma

Same Linux kmod. Install headers for the **running** kernel:

```sh
sudo dnf install kernel-devel-$(uname -r) gcc make elfutils-libelf-devel dkms
sudo ./install.sh
```

Or build a DKMS RPM from this tree (needs `rpm-build`):

```sh
./packaging/fedora/build-rpm.sh
sudo dnf install ~/rpmbuild/RPMS/noarch/t150-dkms-*.rpm
```

`install.sh` and the RPM run `restorecon` on the udev helper so SELinux
allows udev to execute `/usr/lib/udev/hid-t150-rebind`.

**Secure Boot:** the unsigned module will not load until a MOK is enrolled:

```sh
sudo mokutil --import /var/lib/dkms/mok.pub
```

Reboot and complete enrollment in the MOK manager. `install.sh` prints
this reminder when `mokutil --sb-state` says Secure Boot is on.

**Silverblue / Kinoite / ostree:** DKMS and writing under `/usr` are a
poor fit. Use Workstation, a VM, or layer the RPM with `rpm-ostree` at
your own risk.

### FreeBSD

The Linux `hid-t150.ko` cannot load. This tree ships `freebsd/ht150`, a
hidbus module that matches `044f:b677` / `044f:b67f` (and `ht150init` for
the `044f:b65d` boot identity, same GET 73 / SET 83 sequence as Linux
`hid-thrustmaster`).

Needs:

- FreeBSD 13.1+ (usbhid `USB_REQUEST` ioctl for vendor GET/SET)
- Kernel source (`/usr/src/sys` or `SYSDIR`) to build the kmod
- `hw.usb.usbhid.enable=1` in `/boot/loader.conf` (FreeBSD 14; 15 defaults
  usbhid on)
- `EVDEV_SUPPORT` in the kernel (GENERIC has it)

```sh
sudo ./install.sh
```

That builds `/boot/modules/ht150.ko`, installs `ht150ctl`, a devd snippet,
appends `hw.usb.usbhid.enable=1` if missing, and adds `ht150` to
`kld_list`. A reboot is required the first time usbhid is enabled. If
the wheel is already plugged when you `kldload ht150`, unplug and
replug so hidbus re-probes (ht150 beats `hgame` at `BUS_PROBE_DEFAULT`).

Put the user in group `games` so `/dev/input/event*` is writable (same
as `hgame`).

```sh
pw groupmod games -m "$USER"
ht150ctl
ht150ctl range 900
ht150ctl test on    # canned sine; write off to stop
sysctl dev.ht150
```

Sysctl knobs (Oversteer-shaped values):

| Sysctl | Values | Description |
|--------|--------|-------------|
| `dev.ht150.N.wheel.range` | 270–1080 (TMX max 900) | Rotation in degrees |
| `dev.ht150.N.wheel.gain` | 0–65535 | Global FFB intensity |
| `dev.ht150.N.wheel.autocenter` | 0–65535 | Hardware spring-back |
| `dev.ht150.N.wheel.enable_autocenter` | 0 / 1 | Keep hardware autocenter |
| `dev.ht150.N.wheel.spring_level` | 0–100 | Scale applied to `FF_SPRING` |
| `dev.ht150.N.wheel.damper_level` | 0–100 | Scale applied to `FF_DAMPER` |
| `dev.ht150.N.wheel.test_ff` | write 1 / 0 | Play or stop a test sine |
| `dev.ht150.N.wheel.firmware_version` | read-only | Vendor GET 86 byte 1 |
| `dev.ht150.N.wheel.max_range` | read-only | 1080 or 900 |

**Force feedback from games does not work on stock FreeBSD.** In-tree
evdev fakes `EVIOCSFF` / `EVIOCGEFFECTS` and reports no `EV_FF` bits, so
SDL never uploads effects. Axes, buttons, and sysctl/ht150ctl FFB work.
A kernel with a real `EVIOCSFF` path would still need this module; this
tree does not patch evdev.

Packet encode lives in `common/` and is kept in sync with Linux
`hid-t150/forcefeedback.c`. Linux does not `#include` those headers, so
a DKMS build does not need `common/`.

### Manual build

```sh
make
sudo insmod hid-t150/hid-t150.ko
sudo /usr/lib/udev/hid-t150-rebind 0003:044F:B677.xxxx   # after installing the helper

# Packet-size / encode check (no kernel headers)
make test-encode

# FreeBSD kmod (on FreeBSD, with /usr/src/sys)
make -C freebsd
sudo make -C freebsd install
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

**Fedora: module missing with Secure Boot on.** Enroll the DKMS MOK
(`mokutil --import /var/lib/dkms/mok.pub`) and reboot. `dmesg` will say
the module is unsigned until then.

**Fedora: udev rebind blocked by SELinux.** `install.sh` runs
`restorecon` on `/usr/lib/udev/hid-t150-rebind`. If it still fails:
`ausearch -m avc -ts recent | grep hid-t150`.

**FreeBSD: no `/dev/input/event*` for the wheel.** Enable usbhid and
reboot: `sysctl hw.usb.usbhid.enable` should be `1`. `kldstat -v | grep
ht150` should list the module. `hgame` must not own the device;
`ht150` probes at `BUS_PROBE_DEFAULT`.

**FreeBSD: game has no FFB.** Stock evdev stubs `EVIOCSFF`. Use
`ht150ctl test on` to confirm the USB path, and `ht150ctl gain` /
`range` for setup. SDL/Proton FFB needs a patched evdev.

## License

GPL-2.0. See [LICENSE](LICENSE).
