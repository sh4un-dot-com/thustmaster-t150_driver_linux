#include <sys/param.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <dev/hid/hid.h>
#include <dev/hid/hidbus.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_ioctl.h>

#include "t150_packets.h"

/*
 * 044f:b65d "Thrustmaster FFB Wheel" boot identity. Linux hid-thrustmaster
 * identifies the model (GET 73) then switches it (SET 83) so the T150
 * re-enumerates as 044f:b677.
 */

struct tm_wheel_info {
	uint16_t	wheel_type;
	uint16_t	switch_value;
	const char	*wheel_name;
};

static const struct tm_wheel_info tm_wheels_infos[] = {
	{ 0x0306, 0x0006, "Thrustmaster T150RS" },
	{ 0x0200, 0x0005, "Thrustmaster T300RS (Missing Attachment)" },
	{ 0x0206, 0x0005, "Thrustmaster T300RS" },
	{ 0x0209, 0x0005, "Thrustmaster T300RS (Open Wheel Attachment)" },
	{ 0x020a, 0x0005, "Thrustmaster T300RS (Sparco R383 Mod)" },
	{ 0x0204, 0x0005, "Thrustmaster T300 Ferrari Alcantara Edition" },
	{ 0x0002, 0x0002, "Thrustmaster T500RS" },
};

static int ht150init_default_switch = 0x0006;
SYSCTL_INT(_hw, OID_AUTO, ht150init_switch_value, CTLFLAG_RWTUN,
    &ht150init_default_switch, 0,
    "Fallback SET 83 wValue when model GET 73 fails (T150RS is 0x0006)");

static const uint8_t setup_0[] = {
	0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t setup_1[] = {
	0x0a, 0x04, 0x90, 0x03, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t setup_2[] = {
	0x0a, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t setup_3[] = {
	0x0a, 0x04, 0x12, 0x10, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t setup_4[] = {
	0x0a, 0x04, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00
};

struct ht150init_softc {
	device_t	dev;
};

static const struct hid_device_id ht150init_devs[] = {
	{ HID_VP(USB_THRUSTMASTER_VENDOR_ID, USB_THRUSTMASTER_BOOT_ID),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_JOYSTICK) },
	{ HID_VP(USB_THRUSTMASTER_VENDOR_ID, USB_THRUSTMASTER_BOOT_ID),
	  HID_TLC(HUP_GENERIC_DESKTOP, HUG_GAME_PAD) },
};

#ifdef USB_REQUEST
static int
ht150init_vendor(device_t dev, uint8_t type, uint8_t request, uint16_t value,
    void *data, uint16_t len)
{
	struct usb_ctl_request ucr;

	memset(&ucr, 0, sizeof(ucr));
	ucr.ucr_request.bmRequestType = type;
	ucr.ucr_request.bRequest = request;
	USETW(ucr.ucr_request.wValue, value);
	USETW(ucr.ucr_request.wIndex, 0);
	USETW(ucr.ucr_request.wLength, len);
	ucr.ucr_data = data;
#ifdef USB_SHORT_XFER_OK
	if (type & 0x80)
		ucr.ucr_flags = USB_SHORT_XFER_OK;
#endif
	return (hid_ioctl(dev, USB_REQUEST, (uintptr_t)&ucr));
}
#endif

static void
ht150init_setup_interrupts(device_t dev)
{
	(void)hid_write(dev, setup_0, sizeof(setup_0));
	(void)hid_write(dev, setup_1, sizeof(setup_1));
	(void)hid_write(dev, setup_2, sizeof(setup_2));
	(void)hid_write(dev, setup_3, sizeof(setup_3));
	(void)hid_write(dev, setup_4, sizeof(setup_4));
}

static int
ht150init_probe(device_t dev)
{
	int error;

	error = hidbus_lookup_driver_info(dev, ht150init_devs,
	    nitems(ht150init_devs));
	if (error != 0)
		return (error);
	device_set_desc(dev, "Thrustmaster FFB Wheel (init)");
	return (BUS_PROBE_DEFAULT);
}

static int
ht150init_attach(device_t dev)
{
	uint8_t resp[16];
	uint16_t model = 0, switch_value;
	const struct tm_wheel_info *twi = NULL;
	int i, error;

	ht150init_setup_interrupts(dev);

	switch_value = (uint16_t)ht150init_default_switch;
#ifdef USB_REQUEST
	memset(resp, 0, sizeof(resp));
	error = ht150init_vendor(dev, T150_INIT_GET_TYPE, T150_INIT_GET_REQUEST,
	    0, resp, sizeof(resp));
	if (error == 0) {
		uint16_t type = le16toh(*(uint16_t *)resp);

		if (type == 0x49)
			model = le16toh(*(uint16_t *)(resp + 6));
		else if (type == 0x47)
			model = le16toh(*(uint16_t *)(resp + 6));

		for (i = 0; i < (int)nitems(tm_wheels_infos); i++) {
			if (tm_wheels_infos[i].wheel_type == model) {
				twi = &tm_wheels_infos[i];
				break;
			}
		}
		if (twi) {
			device_printf(dev, "model 0x%x is a %s\n",
			    model, twi->wheel_name);
			switch_value = twi->switch_value;
		} else {
			device_printf(dev,
			    "unknown model 0x%x, using switch 0x%x\n",
			    model, switch_value);
		}
	} else {
		device_printf(dev,
		    "model GET 73 failed (%d), using switch 0x%x\n",
		    error, switch_value);
	}

	error = ht150init_vendor(dev, T150_INIT_SET_TYPE, T150_INIT_SET_REQUEST,
	    switch_value, NULL, 0);
	/*
	 * The wheel often disconnects before completing the control transfer,
	 * same as Linux hid-thrustmaster (EPROTO/EPIPE still means success).
	 */
	if (error == 0 || error == EPROTO || error == EPIPE)
		device_printf(dev,
		    "mode switch 0x%x submitted; wheel should re-enumerate\n",
		    switch_value);
	else
		device_printf(dev, "mode switch failed: %d\n", error);
#else
	device_printf(dev,
	    "USB_REQUEST ioctl missing; cannot switch 044f:b65d (need FreeBSD 13.1+)\n");
#endif
	return (0);
}

static int
ht150init_detach(device_t dev)
{
	return (0);
}

static device_method_t ht150init_methods[] = {
	DEVMETHOD(device_probe,		ht150init_probe),
	DEVMETHOD(device_attach,	ht150init_attach),
	DEVMETHOD(device_detach,	ht150init_detach),
	DEVMETHOD_END
};

DEFINE_CLASS_0(ht150init, ht150init_driver, ht150init_methods,
    sizeof(struct ht150init_softc));
DRIVER_MODULE(ht150init, hidbus, ht150init_driver, NULL, NULL);
MODULE_VERSION(ht150init, 1);
MODULE_DEPEND(ht150init, hid, 1, 1, 1);
MODULE_DEPEND(ht150init, hidbus, 1, 1, 1);
MODULE_DEPEND(ht150init, usbhid, 1, 1, 1);
HID_PNP_INFO(ht150init_devs);
