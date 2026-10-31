static int t150_input_configured(struct hid_device *hdev, struct hid_input *hidinput)
{
	struct t150 *t150 = hid_get_drvdata(hdev);
	struct input_dev *input = hidinput->input;
	int axis, errno;

	if (!t150)
		return -ENODEV;

	/* The wheel exposes a single application collection */
	if (t150->joystick)
		return 0;

	t150->joystick = input;
	input_set_drvdata(input, t150);
	input->open = t150_input_open;
	input->close = t150_input_close;

	if (input->absinfo) {
		for (axis = ABS_X; axis <= ABS_BRAKE; axis++) {
			if (!test_bit(axis, input->absbit))
				continue;
			input_abs_set_fuzz(input, axis, 0);
			input_abs_set_flat(input, axis, 0);
		}
	}

	errno = t150_init_ffb(t150);
	if (errno) {
		t150->joystick = NULL;
		return errno;
	}

	return 0;
}

static inline void t150_free_input(struct t150 *t150)
{
}

static int t150_input_open(struct input_dev *dev)
{
	struct t150 *t150 = input_get_drvdata(dev);
	__le16 packet = cpu_to_le16(T150_PKT_INPUT_OPEN);
	int ret;

	ret = t150_send_int(t150, &packet, sizeof(packet));
	if (ret)
		return ret;

	return hid_hw_open(t150->hid_device);
}

static void t150_input_close(struct input_dev *dev)
{
	struct t150 *t150 = input_get_drvdata(dev);
	__le16 what = cpu_to_le16(T150_PKT_INPUT_WHAT);
	__le16 close = cpu_to_le16(T150_PKT_INPUT_CLOSE);
	int i;

	hid_hw_close(t150->hid_device);

	for (i = 0; i < 2; i++)
		t150_send_int(t150, &what, sizeof(what));

	t150_send_int(t150, &close, sizeof(close));
}

/**
 * This function updates the current input status of the joystick
 * @hdev target hid device
 * @packet_raw raw interrupt payload
 */
static int t150_update_input(struct hid_device *hdev, struct hid_report *report,
			     uint8_t *packet_raw, int size)
{
	struct t150 *t150 = hid_get_drvdata(hdev);
	struct t150_state_packet *packet;

	if (!t150 || size < sizeof(struct t150_state_packet))
		return 0;

	packet = (struct t150_state_packet *)packet_raw;

	if (packet->type != STATE_PACKET_INPUT) {
		/* raw_event can run in interrupt context, so defer the USB
		 * control transfer used to refresh settings to process context. */
		t150_schedule_settings_refresh(t150);
		return 0;
	}

	return 0;
}
