/**
 * Send a packet on the interrupt OUT endpoint. The payload is copied into a
 * DMA-safe buffer; never pass stack addresses to usb_interrupt_msg().
 */
static int t150_send_int(struct t150 *t150, const void *data, size_t len)
{
	int actual = 0, ret;

	if (!t150->usb_buf)
		return -ENODEV;
	if (len > t150->usb_buf_size)
		return -EINVAL;

	mutex_lock(&t150->lock);
	if (t150->gone) {
		mutex_unlock(&t150->lock);
		return -ENODEV;
	}

	memcpy(t150->usb_buf, data, len);
	ret = usb_interrupt_msg(t150->usb_device, t150->pipe_out,
				t150->usb_buf, (int)len, &actual, T150_USB_TIMEOUT);
	mutex_unlock(&t150->lock);

	return ret;
}

/**
 * @param t150 ptr to t150
 * @param gain a value between 0x00 and 0xffff where 0xffff is 100% gain
 * @return 0 on success @see usb_interrupt_msg for return codes
 */
static int t150_set_gain(struct t150 *t150, uint16_t gain)
{
	int errno;
	struct ff_change_gain buffer;
	unsigned long flags;

	buffer.f0 = 0x43;
	buffer.gain = cpu_to_le16(gain);

	errno = t150_send_int(t150, &buffer, sizeof(buffer));
	if (!errno) {
		spin_lock_irqsave(&t150->settings.access_lock, flags);
		t150->settings.gain = gain;
		t150->settings.cache_valid = true;
		t150->settings.cache_updated_jiffies = jiffies;
		spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	} else {
		hid_err(t150->hid_device, "set gain failed: %d\n", errno);
	}

	return errno;
}

/**
 * @param autocenter_force a value between 0 and 65535, is the strength of the autocenter effect
 */
static int t150_set_autocenter(struct t150 *t150, uint16_t autocenter_force)
{
	int errno;
	unsigned long flags;

	errno = t150_settings_set40(t150, SET40_RETURN_FORCE,
				    DIV_ROUND_CLOSEST((uint32_t)autocenter_force * 100, 0xffff));
	if (!errno) {
		spin_lock_irqsave(&t150->settings.access_lock, flags);
		t150->settings.autocenter_force = autocenter_force;
		t150->settings.cache_valid = true;
		t150->settings.cache_updated_jiffies = jiffies;
		spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	}

	return errno;
}

/**
 * @param enable true if the autocenter effect is to be kept enabled when the input
 * 	is opened. The autocentering effect is always active while no input are open
 */
static int t150_set_enable_autocenter(struct t150 *t150, bool enable)
{
	int errno;
	unsigned long flags;

	errno = t150_settings_set40(t150, SET40_USE_RETURN_FORCE, enable);
	if (!errno) {
		spin_lock_irqsave(&t150->settings.access_lock, flags);
		t150->settings.autocenter_enabled = enable;
		t150->settings.cache_valid = true;
		t150->settings.cache_updated_jiffies = jiffies;
		spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	}

	return errno;
}

/**
 * @param range a value between 0x0000 and 0xffff where 0xffff is the hardware
 * 	maximum wheel range
 */
static int t150_set_range(struct t150 *t150, uint16_t range)
{
	int errno;
	unsigned long flags;

	errno = t150_settings_set40(t150, SET40_RANGE, range);
	if (!errno) {
		spin_lock_irqsave(&t150->settings.access_lock, flags);
		t150->settings.range = range;
		t150->settings.cache_valid = true;
		t150->settings.cache_updated_jiffies = jiffies;
		spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	}

	return errno;
}

/**
 * @t150 pointer to t150
 * @operation number of operation
 * @argument the argument to pass with the request
 * @return 0 on success @see usb_interrupt_msg for return codes
 */
static int t150_settings_set40(struct t150 *t150, operation_t operation,
			       uint16_t argument)
{
	int errno;
	struct operation40 buffer;

	buffer.code = 0x40;
	buffer.operation = operation;
	buffer.argument = cpu_to_le16(argument);

	errno = t150_send_int(t150, &buffer, sizeof(buffer));
	if (errno)
		hid_err(t150->hid_device,
			"errno %d during operation 0x40 0x%02hhX with argument %04hX\n",
			errno, operation, argument);

	return errno;
}

/* pull the wheel's current configuration block and cache the values */
static int t150_read_settings(struct t150 *t150)
{
	int ret;
	uint8_t buf[8];

	if (!t150->usb_buf)
		return -ENODEV;

	mutex_lock(&t150->lock);
	if (t150->gone) {
		mutex_unlock(&t150->lock);
		return -ENODEV;
	}

	/* The wheel often returns fewer than 8 bytes. usb_control_msg_recv()
	 * turns a short IN into -EREMOTEIO, so use usb_control_msg() and
	 * accept a short read (firmware lives in byte 1). */
	ret = usb_control_msg(t150->usb_device,
			      usb_rcvctrlpipe(t150->usb_device, 0),
			      86, 0xc1, 0, 0,
			      t150->usb_buf, 8,
			      T150_USB_TIMEOUT);
	if (ret >= 2)
		memcpy(buf, t150->usb_buf, min_t(int, ret, 8));
	mutex_unlock(&t150->lock);

	if (ret < 2)
		return ret < 0 ? ret : -EIO;

	spin_lock_irq(&t150->settings.access_lock);
	t150->settings.firmware_version = buf[1];
	if (ret >= 8) {
		t150->settings.gain = make_word(buf[2], buf[3]);
		t150->settings.autocenter_force = make_word(buf[4], buf[5]);
		t150->settings.range = make_word(buf[6], buf[7]);
	}
	t150->settings.cache_valid = true;
	t150->settings.cache_updated_jiffies = jiffies;
	spin_unlock_irq(&t150->settings.access_lock);

	return ret >= 8 ? 0 : 1;
}

/* 0: cache is usable. Negative: no data. Ignores a failed USB refresh
 * when a previous successful write/read already filled the cache. */
static int t150_refresh_for_sysfs(struct t150 *t150)
{
	int ret;
	unsigned long flags;
	bool cache_valid;

	ret = t150_refresh_settings_cached(t150, false);
	if (ret >= 0)
		return 0;

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	cache_valid = t150->settings.cache_valid;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);

	return cache_valid ? 0 : ret;
}

static int t150_refresh_settings_cached(struct t150 *t150, bool force)
{
	unsigned long flags;
	unsigned long cache_updated_jiffies;
	bool cache_valid;

	if (force)
		return t150_read_settings(t150);

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	cache_valid = t150->settings.cache_valid;
	cache_updated_jiffies = t150->settings.cache_updated_jiffies;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);

	if (cache_valid &&
	    time_before(jiffies, cache_updated_jiffies + msecs_to_jiffies(SETTINGS_CACHE_TTL_MS)))
		return 0;

	return t150_read_settings(t150);
}

static void t150_settings_refresh_work(struct work_struct *work)
{
	struct t150 *t150 = container_of(work, struct t150, settings_refresh_work);
	int ret;

	if (t150->gone)
		return;

	ret = t150_refresh_settings_cached(t150, true);
	if (ret < 0)
		hid_warn(t150->hid_device, "unable to refresh wheel settings: %d\n", ret);
}

static void t150_schedule_settings_refresh(struct t150 *t150)
{
	if (!t150->gone)
		schedule_work(&t150->settings_refresh_work);
}

static void t150_cancel_settings_refresh(struct t150 *t150)
{
	cancel_work_sync(&t150->settings_refresh_work);
}

static int t150_get_gain(struct t150 *t150, uint16_t *gain)
{
	unsigned long flags;

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	*gain = t150->settings.gain;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	return 0;
}

static int t150_get_autocenter(struct t150 *t150, uint16_t *autocenter_force)
{
	unsigned long flags;

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	*autocenter_force = t150->settings.autocenter_force;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	return 0;
}

static int t150_get_enable_autocenter(struct t150 *t150, bool *enable)
{
	unsigned long flags;

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	*enable = t150->settings.autocenter_enabled;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	return 0;
}

static int t150_get_range(struct t150 *t150, uint16_t *range)
{
	unsigned long flags;

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	*range = t150->settings.range;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);
	return 0;
}

static int t150_get_max_range(struct t150 *t150, uint16_t *max_range)
{
	if (t150->hid_device->product == USB_T150_PRODUCT_ID)
		*max_range = T150_RANGE_T150_MAX;
	else if (t150->hid_device->product == USB_TMX_PRODUCT_ID)
		*max_range = T150_RANGE_TMX_MAX;
	else
		return -EINVAL;

	return 0;
}

static int t150_setup_task(struct t150 *t150)
{
	int errno;

	errno = t150_refresh_settings_cached(t150, true);
	if (errno == 0) {
		hid_info(t150->hid_device,
			 "settings read from wheel: gain=%u autocenter=%u range=%u fw=%u\n",
			 t150->settings.gain,
			 t150->settings.autocenter_force,
			 t150->settings.range,
			 t150->settings.firmware_version);
		return 0;
	}

	if (errno > 0)
		hid_info(t150->hid_device,
			 "firmware version %u; applying default gain/range/autocenter\n",
			 t150->settings.firmware_version);
	else
		hid_warn(t150->hid_device,
			 "unable to query wheel settings (%d), using defaults\n", errno);

	errno = t150_set_gain(t150, 0xbffe); /* ~75% */
	if (errno)
		hid_err(t150->hid_device, "Error %d while setting the t150 default gain\n", errno);

	errno = t150_set_enable_autocenter(t150, false);
	if (errno)
		hid_err(t150->hid_device,
			"Error %d while setting the t150 default enable_autocenter\n", errno);

	errno = t150_set_autocenter(t150, 0x7fff);
	if (errno)
		hid_err(t150->hid_device,
			"Error %d while setting the t150 default autocenter\n", errno);

	errno = t150_set_range(t150, 0xffff);
	if (errno)
		hid_err(t150->hid_device, "Error %d while setting the t150 default range\n", errno);

	hid_info(t150->hid_device, "Setup completed! Firmware version is %d\n",
		 t150->settings.firmware_version);

	return errno;
}
