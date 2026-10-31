# Thrustmaster T150 and TMX Force Feedback Wheel Linux drivers
**DISCLAMER**
*This is not an official driver from Thrustmaster and is provided without any kind of warranty. Loading and using this driver is at your own risk; I don't take responsibility for kernel panics, devices bricked or any other kind of inconvenience*

## Project status

### What's working
+ All axis and buttons of the wheel are reported
+ Axis fuzz/flat is cleared (avoids a fake center deadzone from the HID descriptor)
+ You can set the range of the T150 wheel from 270° to 1080°
+ You can set the range of the TMX wheel from 270° to 900°
+ You can set the return force of the wheel from 0% to 100%
+ Force feedback
  * Global force feedback scale from 0% to 100% (`gain` sysfs and `FF_GAIN`)
  * Hardware autocenter (`autocenter` sysfs and `FF_AUTOCENTER`)
  * Periodic effects: `FF_SINE`, `FF_SAW_UP`, `FF_SAW_DOWN`
  * Constant effects: `FF_CONSTANT` (including attack/fade envelopes)
  * Condition effects: `FF_SPRING`, `FF_DAMPER`
  * `FF_RUMBLE` is rewritten as a short sine (useful for games that only rumble)
  * Oversteer-style `spring_level` / `damper_level` (0-100) scale condition effects
+ Firmware version is reported
+ Settings are read back from the wheel at probe and on demand
+ Range change events coming from the hardware are handled automatically
+ FFB USB traffic is serialized on a workqueue (safe from input IRQ context)

### What is missing
- Device-side firmware upgrades (`firmware_upgrade` sysfs attribute returns `-EOPNOTSUPP`)
- Extra condition effects (`FF_FRICTION`, `FF_INERTIA`) until the USB codes are confirmed

### Switch the wheel from FFB to full T150
Since kernel version 5.13, hid_thrustmaster automatically switches some Thrustmaster devices into full mode. The T150 is covered by this driver. The TMX wheel does not have support in hid_thrustmaster and requires either using [TMX-Driver](https://github.com/emtek995/TMX-driver) (kernel driver) or [tmdrv](https://github.com/her001/tmdrv) (python userspace driver) to initialize.

## How to use the driver
**For T150, always put the switch of your wheel to the `PS3` position before plugging it into your machine!**
For the TMX, plug the wheel into your machine and load your preferred initialization driver. The mode switch will only be used to swap the clutch and accelerator pedals if using the T3PA-Pro in hangdown mode.

### Setting up the wheel parameters
You can edit the settings of each wheel attached to the machine by writing the sysfs attributes usually found in the
subdirectories at `/sys/devices`. You can see in `dmesg` what path in /sys the input subsystem assigned to the wheel:
for example if you see `input: Thrustmaster T150 steering wheel as /devices/pci0000:00/0000:00:14.0/usb1/1-1/input/input27`
then the attributes will be located at `/sys/devices/pci0000:00/0000:00:14.0/usb1/1-1/input/input27/device/`.

This table contains a summary of each attribute

|Attribute          |Value                         |Description                                                           |
|-------------------|------------------------------|----------------------------------------------------------------------|
|`range`            |decimal from `270` to `1080`  |How far the wheel turns (TMX limited to 900)                          |
|`autocenter`       |decimal from `0` to `65535`   |The force used to re-center the wheel                                 |
|`enable_autocenter`|boolean `y` xor `n`           |Use the user defined return force or let the game handle it through ffb|
|`gain`             |decimal from `0` to `65535`   |Force feedback intensity. 0 no effects are reproduced                 |
|`spring_level`     |decimal from `0` to `100`     |Scale applied to `FF_SPRING` effects (Oversteer compatible)           |
|`damper_level`     |decimal from `0` to `100`     |Scale applied to `FF_DAMPER` effects (Oversteer compatible)           |
|`firmware_version` |decimal                       |Read only, the current firmware running on the wheel                  |
|`max_range`        |decimal                       |Read only, hardware maximum range (T150 1080, TMX 900)              |
|`settings_cache_age_ms`|decimal                 |Read only, age of cached wheel settings in milliseconds (`-1` if unknown) |
|`supported_ff_effects`|comma-separated tokens     |Read only, list of force-feedback effect types exposed by the driver |
|`reload_settings`  |write-any                    |Trigger a probe of the wheel to refresh the cached settings          |
|`firmware_upgrade` |binary (write-only)         |Not implemented; writes fail with `EOPNOTSUPP`                       |

`show` operations use a short settings cache (300ms) to reduce USB control
traffic while keeping values fresh; writing `reload_settings` still forces
an immediate hardware read.

[Oversteer](https://github.com/berarma/oversteer) can drive `range`, `gain`, `autocenter`, `spring_level` and `damper_level`.

### Custom defaults
To automatically set the wheel to some custom default settings when plugged you'll have to write a simple udev rule. In `/etc/udev/rules.d` create a text file called something like `99-t150-defaults.rules` and write a rule like this below. Refer to the output of `udevadm info --attribute-walk /sys/devices/${WHEEL_DEVICE_PATH}` in case rules from example below do not match.
```
SUBSYSTEM=="hid", DRIVER=="hid-t150", ATTR{range}="270", ATTR{gain}="49150"
```
Then run `udevadm control --reload` and `udevadm trigger` to re-load the rules.
The rule in the example should set the turning range to 270°.

## How to install and load the driver
Run `install.sh` as root (`sudo ./install.sh`). The script will:

* build the module (via DKMS when `dkms` is installed, otherwise for the running kernel only)
* install udev rules, a hid-generic rebind helper, `modules-load.d`, and `modprobe.d`
* `modprobe hid_t150` and bind any already-plugged T150/TMX away from `hid-generic`

The in-tree `hid-generic` driver matches every HID device. The T150 (`044f:b677`) and TMX (`044f:b67f`) IDs are not on the kernel's special-driver list, so without this driver they appear as a joystick with **no force feedback**. `install.sh` and the udev rule rebind them to `hid-t150`.

Clang-built kernels (CachyOS, some Fedora/Arch custom kernels) are detected automatically; the Makefile passes `LLVM=1`. Unsigned modules load on kernels that do not enforce signatures (`module.sig_enforce=0`, the CachyOS default).

To check if the module owns the wheel:

```
lsmod | grep hid_t150
ls /sys/bus/hid/drivers/hid-t150
dmesg | grep -i t150
```

To remove everything, run `uninstall.sh` as root. A kernel upgrade without DKMS needs `install.sh` again.

### Quick test script

A helper script at `hid-t150/test_sysfs.sh` finds the device sysfs directory and exercises the attributes (`reload_settings`, `gain`, `spring_level`, etc.). Example usage:

```sh
# from the repo root
./hid-t150/test_sysfs.sh

# to run write tests (gain change) run as root
sudo ./hid-t150/test_sysfs.sh
```

Force feedback can be smoke-tested with `fftest` from linuxconsole tools once the wheel is bound to `hid-t150` (`capabilities/ff` on the input device must be non-zero).

This script is a convenience for manual smoke tests after loading the
module; it does not attempt destructive operations.

### Manually
Copy the udev rules into `/etc/udev/rules.d/` and reload the udev rules (or reboot)...

#### Build the drivers
Install the required tools to compile (like `base-devel`/`build-essential`, matching `linux-headers`) and run
```
make
```
in the repository root. Load the `.ko` file with `insmod` and unload with `rmmod`.
