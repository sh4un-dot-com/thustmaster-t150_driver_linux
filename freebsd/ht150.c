/*-
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Thrustmaster T150/TMX hidbus driver.
 *
 * HID usage map and hat-switch callback follow sys/dev/hid/hgame.c:
 * Copyright (c) 2020 Vladimir Kondratyev <wulf@FreeBSD.org>
 * Copyright (c) 2020 Val Packett <val@packett.cool>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/stat.h>

#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>
#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/hid/hidmap.h>

#include "ht150.h"
#include "t150_presets.h"

char ht150_default_preset[16] = "sim";
SYSCTL_NODE(_hw, OID_AUTO, ht150, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Thrustmaster T150/TMX");
SYSCTL_STRING(_hw_ht150, OID_AUTO, preset, CTLFLAG_RWTUN,
    ht150_default_preset, sizeof(ht150_default_preset),
    "Default feel preset applied at attach");

static evdev_open_t	ht150_ev_open;
static evdev_close_t	ht150_ev_close;
static evdev_event_t	ht150_ev_event;

static int
ht150_hat_switch_cb(HIDMAP_CB_ARGS)
{
	static const struct { int32_t x; int32_t y; } hat_switch_map[] = {
		{ 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 },
		{ 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, 0 }
	};
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();
	u_int idx;

	switch (HIDMAP_CB_GET_STATE()) {
	case HIDMAP_CB_IS_ATTACHING:
		evdev_support_event(evdev, EV_ABS);
		evdev_support_abs(evdev, ABS_HAT0X, -1, 1, 0, 0, 0);
		evdev_support_abs(evdev, ABS_HAT0Y, -1, 1, 0, 0, 0);
		break;
	case HIDMAP_CB_IS_RUNNING:
		idx = MIN(nitems(hat_switch_map) - 1, (u_int)ctx.data);
		evdev_push_abs(evdev, ABS_HAT0X, hat_switch_map[idx].x);
		evdev_push_abs(evdev, ABS_HAT0Y, hat_switch_map[idx].y);
		break;
	default:
		break;
	}
	return (0);
}

static int
ht150_final_cb(HIDMAP_CB_ARGS)
{
	struct evdev_dev *evdev = HIDMAP_CB_GET_EVDEV();

	if (HIDMAP_CB_GET_STATE() == HIDMAP_CB_IS_ATTACHING) {
		evdev_support_prop(evdev, INPUT_PROP_DIRECT);
		evdev_set_cdev_mode(evdev, UID_ROOT, GID_GAMES,
		    S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
	}
	return (ENOSYS);
}

static const struct hidmap_item ht150_map[] = {
	{ HIDMAP_KEY_RANGE(HUP_BUTTON, 1, 16, BTN_TRIGGER) },
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_X, ABS_X) },
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_Y, ABS_Y) },
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_Z, ABS_Z) },
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_RX, ABS_RX) },
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_RY, ABS_RY) },
	{ HIDMAP_ABS(HUP_GENERIC_DESKTOP, HUG_RZ, ABS_RZ) },
	{ HIDMAP_ABS(HUP_SIMULATION, 0xc4, ABS_GAS) },
	{ HIDMAP_ABS(HUP_SIMULATION, 0xc5, ABS_BRAKE) },
	{ HIDMAP_ABS(HUP_SIMULATION, 0xc6, ABS_RUDDER) },
	{ HIDMAP_ABS(HUP_SIMULATION, 0xc8, ABS_WHEEL) },
	{ HIDMAP_ANY_CB(HUP_GENERIC_DESKTOP, HUG_HAT_SWITCH, ht150_hat_switch_cb) },
	{ HIDMAP_KEY_RANGE(HUP_BUTTON, 17, 57, BTN_TRIGGER_HAPPY) },
	{ HIDMAP_FINAL_CB(ht150_final_cb) },
};

static const struct hid_device_id ht150_devs[] = {
	{ HID_VP(USB_THRUSTMASTER_VENDOR_ID, USB_T150_PRODUCT_ID),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_JOYSTICK) },
	{ HID_VP(USB_THRUSTMASTER_VENDOR_ID, USB_TMX_PRODUCT_ID),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_JOYSTICK) },
	{ HID_VP(USB_THRUSTMASTER_VENDOR_ID, USB_T150_PRODUCT_ID),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD) },
	{ HID_VP(USB_THRUSTMASTER_VENDOR_ID, USB_TMX_PRODUCT_ID),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD) },
};

static int
ht150_ev_open(struct evdev_dev *evdev)
{
	device_t dev = evdev_get_softc(evdev);
	struct ht150_softc *sc = device_get_softc(dev);
	uint16_t pkt = htole16(T150_PKT_INPUT_OPEN);
	int error;

	error = ht150_send(sc, &pkt, sizeof(pkt));
	if (error)
		device_printf(dev, "input open packet failed: %d\n", error);
	return (hid_intr_start(dev));
}

static int
ht150_ev_close(struct evdev_dev *evdev)
{
	device_t dev = evdev_get_softc(evdev);
	struct ht150_softc *sc = device_get_softc(dev);
	uint16_t what = htole16(T150_PKT_INPUT_WHAT);
	uint16_t close = htole16(T150_PKT_INPUT_CLOSE);
	int i;

	(void)hid_intr_stop(dev);
	for (i = 0; i < 2; i++)
		(void)ht150_send(sc, &what, sizeof(what));
	(void)ht150_send(sc, &close, sizeof(close));
	return (0);
}

static void
ht150_ev_event(struct evdev_dev *evdev, uint16_t type, uint16_t code,
    int32_t value)
{
	device_t dev = evdev_get_softc(evdev);
	struct ht150_softc *sc = device_get_softc(dev);

	if (type != EV_FF)
		return;

	/*
	 * FreeBSD evdev fakes EVIOCSFF, so games never upload effects.
	 * Gain/autocenter still arrive as EV_FF writes. Playback of a
	 * previously sysctl-uploaded slot is honoured here.
	 */
	if (code == FF_GAIN) {
		(void)ht150_set_gain(sc, (uint16_t)value);
		return;
	}
	if (code == FF_AUTOCENTER) {
		if (value == 0)
			(void)ht150_set_enable_autocenter(sc, 0);
		else {
			(void)ht150_set_enable_autocenter(sc, 1);
			(void)ht150_set_autocenter(sc, (uint16_t)value);
		}
		return;
	}
	if (code < T150_FF_MAX_EFFECTS)
		(void)ht150_ff_play(sc, (int)code, value);
}

static int
ht150_sysctl_range(SYSCTL_HANDLER_ARGS)
{
	struct ht150_softc *sc = arg1;
	int val = sc->range;
	int error;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	return (ht150_set_range_deg(sc, (uint16_t)val));
}

static int
ht150_sysctl_gain(SYSCTL_HANDLER_ARGS)
{
	struct ht150_softc *sc = arg1;
	int val = sc->gain;
	int error;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	if (val < 0)
		val = 0;
	if (val > 0xffff)
		val = 0xffff;
	return (ht150_set_gain(sc, (uint16_t)val));
}

static int
ht150_sysctl_autocenter(SYSCTL_HANDLER_ARGS)
{
	struct ht150_softc *sc = arg1;
	int val = sc->autocenter;
	int error;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	if (val < 0)
		val = 0;
	if (val > 0xffff)
		val = 0xffff;
	return (ht150_set_autocenter(sc, (uint16_t)val));
}

static int
ht150_sysctl_enable_ac(SYSCTL_HANDLER_ARGS)
{
	struct ht150_softc *sc = arg1;
	int val = sc->enable_autocenter;
	int error;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	return (ht150_set_enable_autocenter(sc, val != 0));
}

static int
ht150_sysctl_level(SYSCTL_HANDLER_ARGS)
{
	uint8_t *levelp = arg1;
	int val = *levelp;
	int error;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	if (val < 0)
		val = 0;
	if (val > 100)
		val = 100;
	*levelp = (uint8_t)val;
	return (0);
}

static int
ht150_sysctl_preset(SYSCTL_HANDLER_ARGS)
{
	struct ht150_softc *sc = arg1;
	const struct t150_preset *preset;
	char buf[32];
	int error;

	preset = ht150_matched_preset(sc);
	strlcpy(buf, preset != NULL ? preset->name : "custom", sizeof(buf));
	error = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (error || req->newptr == NULL)
		return (error);
	preset = t150_preset_by_name(buf);
	if (preset == NULL)
		return (EINVAL);
	return (ht150_apply_preset(sc, preset));
}

static int
ht150_sysctl_presets(SYSCTL_HANDLER_ARGS)
{
	char buf[128];
	unsigned int i;
	int n = 0;

	buf[0] = '\0';
	for (i = 0; i < T150_PRESET_COUNT; i++) {
		n += snprintf(buf + n, sizeof(buf) - n, "%s%s",
		    i ? " " : "", t150_presets[i].name);
		if (n < 0 || n >= (int)sizeof(buf))
			break;
	}
	return (sysctl_handle_string(oidp, buf, sizeof(buf), req));
}

static int
ht150_sysctl_test_ff(SYSCTL_HANDLER_ARGS)
{
	struct ht150_softc *sc = arg1;
	int val = 0;
	int error;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || req->newptr == NULL)
		return (error);
	return (ht150_ff_test(sc, val != 0));
}

static void
ht150_sysctl_attach(struct ht150_softc *sc)
{
	struct sysctl_oid *tree;

	sysctl_ctx_init(&sc->sysctl_ctx);
	tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
	    OID_AUTO, "wheel", CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "T150/TMX wheel settings");
	sc->sysctl_tree = tree;

	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "range", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    ht150_sysctl_range, "I", "Rotation in degrees");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "gain", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    ht150_sysctl_gain, "I", "Global FFB gain 0..65535");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "autocenter", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    ht150_sysctl_autocenter, "I", "Autocenter strength 0..65535");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "enable_autocenter", CTLTYPE_INT | CTLFLAG_RW,
	    sc, 0, ht150_sysctl_enable_ac, "I",
	    "Keep hardware autocenter while a game has the device open");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "spring_level", CTLTYPE_INT | CTLFLAG_RW,
	    &sc->spring_level, 0, ht150_sysctl_level, "I",
	    "FF_SPRING scale 0..100");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "damper_level", CTLTYPE_INT | CTLFLAG_RW,
	    &sc->damper_level, 0, ht150_sysctl_level, "I",
	    "FF_DAMPER scale 0..100");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "preset", CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    sc, 0, ht150_sysctl_preset, "A",
	    "Wheel-feel preset (sim arcade street offroad rally drift truck)");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "presets", CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE,
	    sc, 0, ht150_sysctl_presets, "A",
	    "Available wheel-feel presets");
	SYSCTL_ADD_U8(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "firmware_version", CTLFLAG_RD, &sc->firmware_version, 0,
	    "Firmware reported by vendor GET 86");
	SYSCTL_ADD_U16(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "max_range", CTLFLAG_RD, &sc->max_range, 0,
	    "Hardware maximum rotation");
	SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "test_ff", CTLTYPE_INT | CTLFLAG_RW, sc, 0,
	    ht150_sysctl_test_ff, "I",
	    "Write 1 to play a test sine, 0 to stop");
}

static int
ht150_probe(device_t dev)
{
	struct ht150_softc *sc = device_get_softc(dev);
	int error;

	error = HIDMAP_PROBE(&sc->hm, dev, ht150_devs, ht150_map, "T150/TMX");
	if (error > 0)
		return (error);
	return (BUS_PROBE_DEFAULT);
}

static int
ht150_attach(device_t dev)
{
	struct ht150_softc *sc = device_get_softc(dev);
	const struct hid_device_info *hw = hid_get_device_info(dev);
	int error;

	sc->dev = dev;
	sc->product = hw->idProduct;
	sc->max_range = (sc->product == USB_TMX_PRODUCT_ID) ?
	    T150_RANGE_TMX_MAX : T150_RANGE_T150_MAX;
	sc->range = t150_preset_range_deg(T150_PRESET_DEFAULT, sc->max_range);
	sc->gain = T150_PRESET_DEFAULT->gain;
	sc->autocenter = T150_PRESET_DEFAULT->autocenter;
	sc->enable_autocenter = T150_PRESET_DEFAULT->enable_autocenter;
	sc->spring_level = T150_PRESET_DEFAULT->spring_level;
	sc->damper_level = T150_PRESET_DEFAULT->damper_level;
	sc->usb_buf_size = T150_USB_BUF_SIZE;
	sx_init(&sc->lock, "ht150");
	sc->usb_buf = malloc(sc->usb_buf_size, M_DEVBUF, M_WAITOK | M_ZERO);

	error = hidmap_attach(&sc->hm);
	if (error) {
		free(sc->usb_buf, M_DEVBUF);
		sx_destroy(&sc->lock);
		return (error);
	}

	sc->hm.evdev_methods.ev_open = ht150_ev_open;
	sc->hm.evdev_methods.ev_close = ht150_ev_close;
	sc->hm.evdev_methods.ev_event = ht150_ev_event;

	ht150_sysctl_attach(sc);
	(void)ht150_setup(sc);

	device_printf(dev, "T150/TMX wheel ready (fw %u)\n",
	    sc->firmware_version);
	return (0);
}

static int
ht150_detach(device_t dev)
{
	struct ht150_softc *sc = device_get_softc(dev);

	sysctl_ctx_free(&sc->sysctl_ctx);
	(void)hidmap_detach(&sc->hm);
	sx_xlock(&sc->lock);
	sc->gone = 1;
	free(sc->usb_buf, M_DEVBUF);
	sc->usb_buf = NULL;
	sx_xunlock(&sc->lock);
	sx_destroy(&sc->lock);
	return (0);
}

static device_method_t ht150_methods[] = {
	DEVMETHOD(device_probe,		ht150_probe),
	DEVMETHOD(device_attach,	ht150_attach),
	DEVMETHOD(device_detach,	ht150_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_0(ht150, ht150_driver, ht150_methods, sizeof(struct ht150_softc));
DRIVER_MODULE(ht150, hidbus, ht150_driver, NULL, NULL);
MODULE_DEPEND(ht150, hid, 1, 1, 1);
MODULE_DEPEND(ht150, hidbus, 1, 1, 1);
MODULE_DEPEND(ht150, hidmap, 1, 1, 1);
MODULE_DEPEND(ht150, evdev, 1, 1, 1);
MODULE_DEPEND(ht150, usbhid, 1, 1, 1);
MODULE_VERSION(ht150, 1);
HID_PNP_INFO(ht150_devs);
