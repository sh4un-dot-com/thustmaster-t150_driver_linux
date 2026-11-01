#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_ioctl.h>

#include "ht150.h"
#include "t150_ff_encode.h"

int
ht150_send(struct ht150_softc *sc, const void *data, size_t len)
{
	int error;

	if (sc->usb_buf == NULL || len == 0 || len > sc->usb_buf_size)
		return (EINVAL);

	sx_xlock(&sc->lock);
	if (sc->gone) {
		sx_xunlock(&sc->lock);
		return (ENXIO);
	}
	memcpy(sc->usb_buf, data, len);
	error = hid_write(sc->dev, sc->usb_buf, (hid_size_t)len);
	sx_xunlock(&sc->lock);
	return (error);
}

int
ht150_set_gain(struct ht150_softc *sc, uint16_t gain)
{
	struct t150_ff_change_gain pkt;
	int error;

	t150_ff_prepare_gain(&pkt, gain);
	error = ht150_send(sc, &pkt, sizeof(pkt));
	if (error == 0)
		sc->gain = gain;
	else
		device_printf(sc->dev, "set gain failed: %d\n", error);
	return (error);
}

int
ht150_set_autocenter(struct ht150_softc *sc, uint16_t force)
{
	struct t150_operation40 pkt;
	int error;

	t150_prepare_set40(&pkt, SET40_RETURN_FORCE,
	    t150_autocenter_to_percent(force));
	error = ht150_send(sc, &pkt, sizeof(pkt));
	if (error == 0)
		sc->autocenter = force;
	return (error);
}

int
ht150_set_enable_autocenter(struct ht150_softc *sc, int enable)
{
	struct t150_operation40 pkt;
	int error;

	t150_prepare_set40(&pkt, SET40_USE_RETURN_FORCE, enable ? 1 : 0);
	error = ht150_send(sc, &pkt, sizeof(pkt));
	if (error == 0)
		sc->enable_autocenter = enable ? 1 : 0;
	return (error);
}

int
ht150_set_range_deg(struct ht150_softc *sc, uint16_t degrees)
{
	struct t150_operation40 pkt;
	uint16_t wire;
	int error;

	wire = t150_range_to_wire(degrees, sc->max_range);
	t150_prepare_set40(&pkt, SET40_RANGE, wire);
	error = ht150_send(sc, &pkt, sizeof(pkt));
	if (error == 0)
		sc->range = t150_range_from_wire(wire, sc->max_range);
	return (error);
}

#ifdef USB_REQUEST
static int
ht150_vendor_get(struct ht150_softc *sc, uint8_t *buf, uint16_t len)
{
	struct usb_ctl_request ucr;

	memset(&ucr, 0, sizeof(ucr));
	memset(buf, 0, len);
	ucr.ucr_request.bmRequestType = T150_SETTINGS_GET_TYPE;
	ucr.ucr_request.bRequest = T150_SETTINGS_GET_REQUEST;
	USETW(ucr.ucr_request.wValue, 0);
	USETW(ucr.ucr_request.wIndex, 0);
	USETW(ucr.ucr_request.wLength, len);
	ucr.ucr_data = buf;
#ifdef USB_SHORT_XFER_OK
	ucr.ucr_flags = USB_SHORT_XFER_OK;
#endif
	return (hid_ioctl(sc->dev, USB_REQUEST, (uintptr_t)&ucr));
}
#endif

int
ht150_read_settings(struct ht150_softc *sc)
{
#ifdef USB_REQUEST
	uint8_t buf[8];
	int error;

	sx_xlock(&sc->lock);
	if (sc->gone) {
		sx_xunlock(&sc->lock);
		return (ENXIO);
	}
	/*
	 * usbhid(4) reports actlen as the requested length, and may
	 * reject a short IN. Try 8 bytes, then 2 for firmware only.
	 * An all-zero settings block is the usual short-read padding.
	 */
	error = ht150_vendor_get(sc, buf, 8);
	if (error) {
		error = ht150_vendor_get(sc, buf, 2);
		sx_xunlock(&sc->lock);
		if (error)
			return (error);
		if (buf[1] == 0)
			return (EIO);
		sc->firmware_version = buf[1];
		sc->cache_valid = 1;
		return (EAGAIN);
	}
	sx_xunlock(&sc->lock);

	sc->firmware_version = buf[1];
	if (buf[1] == 0)
		return (EIO);
	if (buf[2] == 0 && buf[3] == 0 && buf[4] == 0 && buf[5] == 0 &&
	    buf[6] == 0 && buf[7] == 0) {
		sc->cache_valid = 1;
		return (EAGAIN);
	}
	sc->gain = t150_make_word(buf[2], buf[3]);
	sc->autocenter = t150_make_word(buf[4], buf[5]);
	sc->range = t150_range_from_wire(
	    t150_make_word(buf[6], buf[7]), sc->max_range);
	sc->cache_valid = 1;
	return (0);
#else
	return (EOPNOTSUPP);
#endif
}

int
ht150_setup(struct ht150_softc *sc)
{
	int error;

	error = ht150_read_settings(sc);
	if (error == 0) {
		device_printf(sc->dev,
		    "settings from wheel: gain=%u autocenter=%u range=%u fw=%u\n",
		    sc->gain, sc->autocenter, sc->range, sc->firmware_version);
		return (0);
	}

	if (error == EAGAIN)
		device_printf(sc->dev,
		    "firmware version %u; applying default gain/range/autocenter\n",
		    sc->firmware_version);
	else
		device_printf(sc->dev,
		    "unable to query wheel settings (%d), using defaults\n",
		    error);

	(void)ht150_set_gain(sc, 0xbffe);
	(void)ht150_set_enable_autocenter(sc, 0);
	(void)ht150_set_autocenter(sc, 0x7fff);
	(void)ht150_set_range_deg(sc, sc->max_range);
	sc->cache_valid = 1;
	return (0);
}

int
ht150_ff_upload(struct ht150_softc *sc, struct ff_effect *effect)
{
	struct t150_ff_first first;
	struct t150_ff_update update;
	struct t150_ff_commit commit;
	struct ff_effect local = *effect;
	int error;

	if (local.id < 0 || local.id >= T150_FF_MAX_EFFECTS)
		return (EINVAL);
	if (local.type == FF_PERIODIC && local.u.periodic.period == 0)
		return (EINVAL);

	t150_ff_rewrite_rumble(&local);
	t150_ff_prepare_first(&first, &local);
	t150_ff_prepare_update(&update, &local, sc->spring_level,
	    sc->damper_level);
	t150_ff_prepare_commit(&commit, &local);

	error = ht150_send(sc, &first, sizeof(first));
	if (error)
		return (error);
	error = ht150_send(sc, &update, sizeof(update));
	if (error)
		return (error);
	error = ht150_send(sc, &commit, sizeof(commit));
	if (error)
		return (error);

	sc->effects[local.id] = local;
	sc->effect_used[local.id] = 1;
	return (0);
}

int
ht150_ff_play(struct ht150_softc *sc, int effect_id, int times)
{
	struct t150_ff_change_effect_status pkt;

	if (effect_id < 0 || effect_id >= T150_FF_MAX_EFFECTS)
		return (EINVAL);
	t150_ff_prepare_play(&pkt, effect_id, times);
	return (ht150_send(sc, &pkt, sizeof(pkt)));
}

int
ht150_ff_test(struct ht150_softc *sc, int enable)
{
	struct ff_effect effect;
	int error;

	if (!enable)
		return (ht150_ff_play(sc, 0, 0));

	memset(&effect, 0, sizeof(effect));
	effect.type = FF_PERIODIC;
	effect.id = 0;
	effect.direction = 16384;
	effect.replay.length = 0;
	effect.u.periodic.waveform = FF_SINE;
	effect.u.periodic.period = 50;
	effect.u.periodic.magnitude = 0x7fff;
	error = ht150_ff_upload(sc, &effect);
	if (error)
		return (error);
	return (ht150_ff_play(sc, 0, 1));
}
