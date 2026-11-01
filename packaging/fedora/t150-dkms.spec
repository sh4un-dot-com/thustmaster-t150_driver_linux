# DKMS package for hid-t150. Build from the repository with:
#   ./packaging/fedora/build-rpm.sh
#
# Name:           t150-dkms
# This is a noarch source package; DKMS builds hid-t150.ko per kernel.

Name:           t150-dkms
Version:        1.1
Release:        1%{?dist}
Summary:        DKMS source for the Thrustmaster T150/TMX HID force-feedback driver
License:        GPL-2.0-only
URL:            https://github.com/sh4un-dot-com/t150_driver_x
Source0:        t150-%{version}.tar.gz

BuildArch:      noarch
BuildRequires:  systemd-rpm-macros
Requires:       dkms
Requires:       gcc
Requires:       make
Requires:       kernel-devel
Requires:       elfutils-libelf-devel
Requires:       udev

%description
Out-of-tree Linux HID driver for the Thrustmaster T150/T150RS (044f:b677)
and TMX (044f:b67f). DKMS rebuilds hid-t150.ko when the kernel is upgraded.

The package also installs udev rules that rebind the wheel off hid-generic
(so force feedback is available), sysfs knobs used by Oversteer, and the
t150-preset helper for sim/arcade/street/offroad/rally/drift/truck feel.

Secure Boot: enroll the DKMS Machine Owner Key after install:
  sudo mokutil --import /var/lib/dkms/mok.pub
then reboot and complete the MOK enrollment.

Silverblue/Kinoite (ostree) cannot use DKMS in the usual way.

%prep
%setup -q -n t150-%{version}

%build
# DKMS builds the kmod on the installed system.

%install
install -d %{buildroot}%{_usrsrc}/t150-%{version}/hid-t150
install -d %{buildroot}%{_usrsrc}/t150-%{version}/build
install -d %{buildroot}%{_usrsrc}/t150-%{version}/common
install -m 644 hid-t150/*.c hid-t150/*.h hid-t150/Makefile \
        %{buildroot}%{_usrsrc}/t150-%{version}/hid-t150/
install -m 644 common/t150_presets.h \
        %{buildroot}%{_usrsrc}/t150-%{version}/common/
install -m 644 dkms_make.mak %{buildroot}%{_usrsrc}/t150-%{version}/Makefile
install -m 644 dkms.conf %{buildroot}%{_usrsrc}/t150-%{version}/

install -D -m 755 files/usr/lib/udev/hid-t150-rebind \
        %{buildroot}%{_prefix}/lib/udev/hid-t150-rebind
install -D -m 755 files/usr/bin/t150-preset \
        %{buildroot}%{_bindir}/t150-preset
install -D -m 644 files/etc/udev/rules.d/10-t150.rules \
        %{buildroot}%{_udevrulesdir}/10-t150.rules
install -D -m 644 files/etc/modules-load.d/hid-t150.conf \
        %{buildroot}%{_modulesloaddir}/hid-t150.conf
install -D -m 644 files/etc/modprobe.d/hid-t150.conf \
        %{buildroot}%{_modprobedir}/hid-t150.conf
install -D -m 644 files/etc/udev/rules.d/99-t150-defaults.rules \
        %{buildroot}%{_sysconfdir}/udev/rules.d/99-t150-defaults.rules

%post
dkms add -m t150 -v %{version} --rpm_safe_upgrade >/dev/null 2>&1 || :
dkms build -m t150 -v %{version} >/dev/null 2>&1 || :
dkms install -m t150 -v %{version} --force >/dev/null 2>&1 || :
modprobe hid-t150 >/dev/null 2>&1 || :
if command -v udevadm >/dev/null 2>&1; then
        udevadm control --reload >/dev/null 2>&1 || :
        udevadm trigger --subsystem-match=hid >/dev/null 2>&1 || :
fi
if command -v restorecon >/dev/null 2>&1; then
        restorecon -F %{_prefix}/lib/udev/hid-t150-rebind \
                %{_bindir}/t150-preset \
                %{_udevrulesdir}/10-t150.rules \
                %{_modulesloaddir}/hid-t150.conf \
                %{_modprobedir}/hid-t150.conf \
                >/dev/null 2>&1 || :
fi
if command -v mokutil >/dev/null 2>&1 && mokutil --sb-state 2>/dev/null | grep -qi 'SecureBoot enabled'; then
        echo "Secure Boot is enabled. Enroll the DKMS MOK:" >&2
        echo "  sudo mokutil --import /var/lib/dkms/mok.pub" >&2
        echo "then reboot and finish enrollment in the MOK manager." >&2
fi

%preun
if [ "$1" -eq 0 ]; then
        rmmod hid_t150 >/dev/null 2>&1 || true
        dkms remove -m t150 -v %{version} --all --rpm_safe_upgrade >/dev/null 2>&1 || :
fi

%postun
if command -v udevadm >/dev/null 2>&1; then
        udevadm control --reload >/dev/null 2>&1 || :
fi
if command -v depmod >/dev/null 2>&1; then
        depmod -a >/dev/null 2>&1 || :
fi

%files
%license LICENSE
%doc README.md
%{_usrsrc}/t150-%{version}/
%{_prefix}/lib/udev/hid-t150-rebind
%{_bindir}/t150-preset
%{_udevrulesdir}/10-t150.rules
%{_modulesloaddir}/hid-t150.conf
%{_modprobedir}/hid-t150.conf
%config(noreplace) %{_sysconfdir}/udev/rules.d/99-t150-defaults.rules

%changelog
* Sat Oct 31 2026 t150_driver_x <none@none> - 1.1-1
- Initial Fedora DKMS package for hid-t150 1.1.
