# Thrustmaster T150 / TMX force-feedback driver

Out-of-tree HID driver for the Thrustmaster T150, T150RS, and TMX racing
wheels. On Linux the module is `hid-t150`. On FreeBSD it is a hidbus
kmod named `ht150`.

This is a fork of [scarburato/t150_driver](https://github.com/scarburato/t150_driver)
with a modern-kernel rewrite of the Linux FFB path, Oversteer sysfs,
udev rebind off `hid-generic`, Fedora packaging, and a FreeBSD port.

**This is not an official Thrustmaster driver.** There is no warranty.
Loading it is at your own risk.

## Tested status

**Linux works.** It was installed, loaded, and used on this hardware:

| Item | Value |
|------|--------|
| Host | CachyOS Linux 7.3.0-4-cachyos (`x86_64`, Clang-built kernel) |
| Wheel | Thrustmaster T150RS, firmware **35**, mode switch **PS3** |
| USB ID after init | `044f:b677` |
| HID device | `0003:044F:B677.0028` bound to **hid-t150** |
| Input | `Thrustmaster Thrustmaster T150RS` (`input94`) |
| Sysfs | `firmware_version=35`, `range=1080`, `gain=65535`, `spring_level=100`, `damper_level=100` |
| FFB bits | `FF_GAIN`, `FF_AUTOCENTER`, `FF_PERIODIC`, `FF_SINE`, `FF_SAW_UP`, `FF_SAW_DOWN`, `FF_RUMBLE`, `FF_CONSTANT`, `FF_SPRING`, `FF_DAMPER` |

On that host the wheel enumerates, `hid-generic` is stolen by the udev
rebind helper, axes/buttons report, sysfs knobs read and write, and
Linux force-feedback uploads go out on the interrupt OUT endpoint.

| Platform | Driver | Hardware-tested in this tree |
|----------|--------|------------------------------|
| CachyOS / Arch Linux | `hid-t150` | **Yes** — T150RS firmware 35 |
| Fedora / RHEL-like | same `hid-t150` kmod | Install/DKMS/SELinux/Secure Boot support is in-tree; no Fedora box was used here |
| Other Linux | same kmod | Expected to match CachyOS if headers and udev work |
| FreeBSD 13.1+ | `ht150.ko` | Sources and install script are in-tree; **not** built or plugged in on FreeBSD here |

## Hardware

| Wheel | USB ID after init | Rotation | Mode / init |
|-------|-------------------|----------|-------------|
| T150 / T150RS | `044f:b677` | 270°–1080° | Slide the switch to **PS3** before plugging in |
| TMX | `044f:b67f` | 270°–900° | Linux: [TMX-Driver](https://github.com/emtek995/TMX-driver) or [tmdrv](https://github.com/her001/tmdrv) first. FreeBSD: `ht150init` in `ht150.ko` |

A T150 first appears as boot identity `044f:b65d` (“Thrustmaster FFB
Wheel”). From Linux 5.13 the in-tree `hid-thrustmaster` driver sends
vendor GET 73 / SET 83 and the wheel re-enumerates as `044f:b677`. This
out-of-tree module then owns FFB and the Oversteer knobs. TMX init is
still a separate step on Linux.

Always plug the wheel **directly** into a USB port on the machine.

## What the Linux driver does

### Working (verified on the T150RS above)

- Wheel, pedals, and buttons through the normal HID input path
- HID descriptor fuzz/flat cleared so there is no fake center deadzone
- Force feedback uploaded from a workqueue (safe from input IRQ / atomic
  `set_gain` / `playback` context)
  - `FF_GAIN`, `FF_AUTOCENTER`
  - `FF_CONSTANT` (attack/fade envelopes, direction via a Q15 sine)
  - `FF_SINE`, `FF_SAW_UP`, `FF_SAW_DOWN`
  - `FF_SPRING`, `FF_DAMPER` (scaled by `spring_level` / `damper_level`, default 100)
  - `FF_RUMBLE` rewritten as a short east-facing sine (period 50)
- Up to **9** simultaneous effects. Packet IDs are 8-bit
  (`id * 0x1c + 0x1c`); id 9 overflows
- Sysfs knobs compatible with [Oversteer](https://github.com/berarma/oversteer)
- Firmware version from vendor GET request 86 (byte 1)
- Automatic rebind off `hid-generic` so FFB is actually available

`hid-generic` matches every HID device, and T150/TMX are not on the
in-tree `HID_QUIRK_HAVE_SPECIAL_DRIVER` list. Without the udev helper
the wheel shows up as a joystick with `ff=0`.
`/usr/lib/udev/hid-t150-rebind` unbinds `hid-generic` and binds
`hid-t150`.

### Known gaps

- `firmware_upgrade` always returns `-EOPNOTSUPP`. Use Thrustmaster’s
  Windows updater if you need a firmware flash.
- `FF_FRICTION` and `FF_INERTIA` are **not advertised** and not
  implemented. Spring and damper commit codes (`0x4040`, `0x4041`) are
  known from USB captures. Friction/inertia would need their own commit
  codes from a Windows capture; guessing them is unsafe. Games that
  query `EV_FF` skip those two types. Typical racing FFB (constant,
  periodic, spring, damper, rumble) does not need them.
- Vendor GET 86 often returns a **short** IN (firmware in byte 1, not a
  full 8-byte settings block). Probe still applies default gain (~75%,
  `0xbffe`), full range, and autocenter off. After that, sysfs reads use
  the driver’s cache. Writes always go out on interrupt OUT.

## Install (Linux)

Needs matching kernel headers and a compiler. Clang-built kernels
(CachyOS and some custom kernels) are detected from
`CONFIG_CC_IS_CLANG`; the Makefile then passes `LLVM=1`. Stock Fedora
uses GCC.

```sh
sudo ./install.sh
```

On Linux the script:

1. Builds the module — DKMS when `dkms` is installed, otherwise for the
   running kernel only (`extra/hid-t150`)
2. Installs udev rules, `/usr/lib/udev/hid-t150-rebind`,
   `modules-load.d`, and `modprobe.d`
3. Relabels those files with `restorecon` when SELinux is present
4. Loads `hid-t150` and rebinds any already-plugged T150/TMX
5. Prints a MOK reminder when `mokutil` says Secure Boot is on

On FreeBSD the same command runs `freebsd/install.sh`.

### Dependencies

```sh
# Arch / CachyOS
sudo pacman -S linux-headers gcc make          # or linux-cachyos-headers
sudo pacman -S dkms                            # optional, recommended

# Fedora / RHEL / CentOS Stream / Rocky / Alma
sudo dnf install kernel-devel-$(uname -r) gcc make elfutils-libelf-devel
sudo dnf install dkms                          # optional, recommended

# Debian / Ubuntu
sudo apt install linux-headers-$(uname -r) gcc make
sudo apt install dkms                          # optional, recommended
```

Install DKMS if you want the module rebuilt on kernel upgrades.
Without it, run `sudo ./install.sh` again after a kernel update.

### Unsigned modules

Distro kernels are signed; headers do not ship a private key. A
`sign-file: certs/signing_key.pem` line during `modules_install` is
normal. The module still installs (often as `hid-t150.ko.zst`).

When `module.sig_enforce=0` (CachyOS default) the unsigned module
loads and `dmesg` may report `module verification failed: signature
and/or required key missing` and taint the kernel. That is expected.

With **Secure Boot** the module will not load until a Machine Owner
Key is enrolled (see Fedora below).

### Confirm the driver owns the wheel

```sh
lsmod | grep hid_t150
ls /sys/bus/hid/drivers/hid-t150
dmesg | grep -i t150
./hid-t150/test_sysfs.sh
```

You want a hid name such as `0003:044F:B677.xxxx` under `hid-t150`,
`firmware_version` readable, and `capabilities/ff` non-zero on the
input device.

```sh
ls -l /sys/bus/hid/devices/0003:044F:B677.*/driver
# .../drivers/hid-t150
```

Force feedback: `fftest /dev/input/eventN` from linuxconsole tools, or
any game that uses Linux FF (SDL, Proton with the Linux evdev path,
Oversteer’s test).

Uninstall:

```sh
sudo ./uninstall.sh
```

### Manual build

```sh
make
sudo insmod hid-t150/hid-t150.ko
sudo /usr/lib/udev/hid-t150-rebind 0003:044F:B677.xxxx
```

`insmod` alone leaves an already-probed wheel on `hid-generic`. Prefer
`install.sh`. The hid kernel name is under `/sys/bus/hid/devices`.

Host-side packet-size / encode check (no kernel headers):

```sh
make test-encode
```

## Sysfs and Oversteer

Attributes live on the **HID** device, not the input node:

```
/sys/bus/hid/devices/0003:044F:B677.*/
/sys/bus/hid/devices/0003:044F:B67F.*/
```

`dmesg` prints the path, for example:

```
hid-t150 0003:044F:B677.0028: T150/TMX wheel ready
```

The kernel forbids world-writable `DEVICE_ATTR` modes, so the driver
creates knobs as `0664` and udev chmods them to `0666` after bind.
Oversteer can then write them as a normal user.

| Attribute | Values | Description |
|-----------|--------|-------------|
| `range` | 270–1080 (TMX max 900) | Wheel rotation in degrees |
| `gain` | 0–65535 | Global FFB intensity |
| `autocenter` | 0–65535 | Hardware spring-back strength |
| `enable_autocenter` | `y` / `n` | Keep hardware autocenter while a game has the device open |
| `spring_level` | 0–100 | Scale applied to `FF_SPRING` |
| `damper_level` | 0–100 | Scale applied to `FF_DAMPER` |
| `firmware_version` | integer, read-only | Firmware from vendor GET 86 |
| `max_range` | 1080 or 900, read-only | Hardware maximum rotation |
| `settings_cache_age_ms` | integer or `-1` | Age of the cached settings |
| `supported_ff_effects` | tokens, read-only | Effects this driver exposes |
| `reload_settings` | write-only, root | Force a vendor GET (short reads still succeed) |
| `firmware_upgrade` | write-only | Always `-EOPNOTSUPP` |

Reads use a 300 ms cache so Oversteer polling does not hammer USB.
Writes go to the wheel immediately and update the cache.

Oversteer drives `range`, `gain`, `autocenter`, `spring_level`, and
`damper_level`.

### Plug-in defaults

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

## Fedora / RHEL-like

Same Linux kmod. Install headers for the **running** kernel
(`uname -r` must match `kernel-devel`).

```sh
sudo dnf install kernel-devel-$(uname -r) gcc make elfutils-libelf-devel dkms
sudo ./install.sh
```

DKMS RPM from this tree (needs `rpm-build`):

```sh
./packaging/fedora/build-rpm.sh
sudo dnf install ~/rpmbuild/RPMS/noarch/t150-dkms-*.rpm
```

`install.sh` and the RPM run `restorecon` on
`/usr/lib/udev/hid-t150-rebind` so SELinux allows udev to execute it.

**Secure Boot:** enroll the DKMS MOK, reboot, and finish enrollment in
the MOK manager:

```sh
sudo mokutil --import /var/lib/dkms/mok.pub
```

`install.sh` prints this when `mokutil --sb-state` says Secure Boot is
on. Until the key is enrolled, `dmesg` will say the module is unsigned
and it will not load.

**Silverblue / Kinoite / ostree:** DKMS and writing under `/usr` are a
poor fit. Use Workstation, a VM, or layer the RPM with `rpm-ostree` at
your own risk.

## FreeBSD

The Linux `hid-t150.ko` cannot load on FreeBSD. This tree ships
`freebsd/ht150`, a hidbus module for `044f:b677` / `044f:b67f`, plus
`ht150init` for the `044f:b65d` boot identity (same GET 73 / SET 83
sequence as Linux `hid-thrustmaster`).

It has **not** been hardware-tested on FreeBSD in this tree. Packet
encode in `common/` matches the Linux driver that *was* tested.

Needs:

- FreeBSD 13.1+ (usbhid `USB_REQUEST` ioctl for vendor GET/SET)
- Kernel source (`/usr/src/sys` or `SYSDIR`) to build the kmod
- `hw.usb.usbhid.enable=1` in `/boot/loader.conf` (required on 14;
  15 defaults usbhid on)
- `EVDEV_SUPPORT` in the kernel (GENERIC has it)

```sh
sudo ./install.sh
```

That builds `/boot/modules/ht150.ko`, installs `ht150ctl` and a devd
snippet, appends `hw.usb.usbhid.enable=1` if missing, and adds `ht150`
to `kld_list`. Reboot the first time usbhid is enabled. If the wheel is
already plugged when you `kldload ht150`, unplug and replug so hidbus
re-probes (`ht150` beats `hgame` at `BUS_PROBE_DEFAULT`).

Put the user in group `games` so `/dev/input/event*` is writable (same
as `hgame`):

```sh
pw groupmod games -m "$USER"
ht150ctl
ht150ctl range 900
ht150ctl test on     # canned sine; `ht150ctl test off` to stop
sysctl dev.ht150
```

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

**Game FFB through `/dev/input/event*` does not work on stock
FreeBSD.** In-tree evdev fakes `EVIOCSFF` / `EVIOCGEFFECTS` and reports
no `EV_FF` bits, so SDL never uploads effects. Axes, buttons, and
sysctl / `ht150ctl` FFB are the working path. This tree does not patch
evdev.

Linux `hid-t150` does not `#include` `common/`, so a DKMS build does
not need that directory. Keep `common/t150_packets.h` in sync with
`hid-t150/forcefeedback.h` by hand.

```sh
make -C freebsd          # on FreeBSD, with /usr/src/sys
sudo make -C freebsd install
```

## Troubleshooting

**Joystick works, FFB does not.** The device is still on `hid-generic`.

```sh
ls -l /sys/bus/hid/devices/0003:044F:B677.*/driver
sudo /usr/lib/udev/hid-t150-rebind "$(basename /sys/bus/hid/devices/0003:044F:B677.*)"
```

The driver symlink should end in `hid-t150`.

**Wheel stays `044f:b65d`.** T150: set the switch to PS3, unplug,
replug. Confirm `hid-thrustmaster` ran (`dmesg | grep -i thrust`).
TMX: load TMX-Driver or tmdrv first.

**Module missing after a reboot or kernel update.**
`lsmod | grep hid_t150` empty: `sudo ./install.sh` again. Install
`dkms` so upgrades rebuild it.

**`sign-file: certs/signing_key.pem` during install.** Distro kernel is
signed; headers have no private key. Harmless when
`module.sig_enforce` is `N`.

**Fedora, Secure Boot on, module missing.** Enroll the DKMS MOK
(`mokutil --import /var/lib/dkms/mok.pub`) and reboot.

**Fedora, udev rebind blocked by SELinux.** `install.sh` runs
`restorecon` on the helper. If it still fails:
`ausearch -m avc -ts recent | grep hid-t150`.

**FreeBSD, no `/dev/input/event*` for the wheel.** Enable usbhid and
reboot: `sysctl hw.usb.usbhid.enable` should be `1`.
`kldstat -v | grep ht150` should list the module.

**FreeBSD, game has no FFB.** Stock evdev stubs `EVIOCSFF`. Use
`ht150ctl test on` to confirm the USB path.

## Layout

```
hid-t150/          Linux HID driver (version 1.1)
files/             udev, modules-load.d, modprobe.d, rebind helper
freebsd/           hidbus kmod ht150 + ht150ctl
common/            portable packet structs / encode (FreeBSD + host tests)
packaging/fedora/  t150-dkms.spec and build-rpm.sh
install.sh         Linux or FreeBSD
```

DKMS package name is `t150`, version `1.1`. The module installs under
`/lib/modules/$(uname -r)/extra/hid-t150/`.

## License

GPL-2.0. See [LICENSE](LICENSE).
