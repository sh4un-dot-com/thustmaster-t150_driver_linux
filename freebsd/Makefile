# Out-of-tree hidbus driver for Thrustmaster T150/TMX.
# Needs FreeBSD kernel source (SYSDIR or /usr/src/sys).
# GNU make on Linux will not build this; use bmake on FreeBSD:
#   make -C freebsd
# or: sudo ./install.sh

.if !defined(SYSDIR)
.for _dir in ${SRCTOP:D${SRCTOP}/sys} /usr/src/sys /sys
.if !defined(SYSDIR) && exists(${_dir}/kern/) && exists(${_dir}/conf/kmod.mk)
SYSDIR=	${_dir:tA}
.endif
.endfor
.endif

.if !defined(SYSDIR) || !exists(${SYSDIR}/conf/kmod.mk)
.error Unable to locate the kernel source tree. Set SYSDIR=/path/to/sys
.endif

KMOD=	ht150
SRCS=	ht150.c ht150_ff.c ht150init.c
SRCS+=	opt_hid.h opt_evdev.h opt_usb.h
SRCS+=	bus_if.h device_if.h hid_if.h
CFLAGS+= -I${.CURDIR}/../common -I${.CURDIR} -DEVDEV_SUPPORT

.include "${SYSDIR}/conf/kmod.mk"
