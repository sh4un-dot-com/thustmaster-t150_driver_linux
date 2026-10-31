static struct device_attribute *t150_attrs[] = {
	&dev_attr_autocenter,
	&dev_attr_enable_autocenter,
	&dev_attr_range,
	&dev_attr_gain,
	&dev_attr_firmware_version,
	&dev_attr_max_range,
	&dev_attr_settings_cache_age_ms,
	&dev_attr_supported_ff_effects,
	&dev_attr_spring_level,
	&dev_attr_damper_level,
	&dev_attr_reload_settings,
	&dev_attr_firmware_upgrade,
};

static inline int t150_init_attributes(struct t150 *t150)
{
	int i, errno;

	t150_setup_task(t150);

	for (i = 0; i < ARRAY_SIZE(t150_attrs); i++) {
		errno = device_create_file(&t150->hid_device->dev, t150_attrs[i]);
		if (errno) {
			while (--i >= 0)
				device_remove_file(&t150->hid_device->dev, t150_attrs[i]);
			return errno;
		}
	}

	return 0;
}

static inline void t150_free_attributes(struct t150 *t150)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(t150_attrs); i++)
		device_remove_file(&t150->hid_device->dev, t150_attrs[i]);
}

static ssize_t t150_store_return_force(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	uint16_t nforce;
	struct t150 *t150 = dev_get_drvdata(dev);
	int errno;

	if (kstrtou16(buf, 10, &nforce))
		return -EINVAL;

	errno = t150_set_autocenter(t150, nforce);
	if (errno)
		return errno;

	return count;
}

static ssize_t t150_show_return_force(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	uint16_t val = 0;

	if (t150_refresh_for_sysfs(t150) < 0)
		return -EIO;

	t150_get_autocenter(t150, &val);
	return sysfs_emit(buf, "%u\n", val);
}

static ssize_t t150_store_simulate_return_force(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	bool use;
	struct t150 *t150 = dev_get_drvdata(dev);
	int errno;

	if (kstrtobool(buf, &use))
		return -EINVAL;

	errno = t150_set_enable_autocenter(t150, use);
	if (errno)
		return errno;

	return count;
}

static ssize_t t150_show_simulate_return_force(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	bool en = false;

	if (t150_refresh_for_sysfs(t150) < 0)
		return -EIO;

	t150_get_enable_autocenter(t150, &en);
	return sysfs_emit(buf, "%c\n", en ? 'y' : 'n');
}

static ssize_t t150_store_range(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	uint16_t range;
	uint16_t dev_max_range = 0;
	struct t150 *t150 = dev_get_drvdata(dev);
	int errno;

	if (kstrtou16(buf, 10, &range))
		return -EINVAL;

	if (t150_get_max_range(t150, &dev_max_range) < 0)
		return -EINVAL;

	if (range < T150_RANGE_MIN)
		range = T150_RANGE_MIN;
	else if (range > dev_max_range)
		range = dev_max_range;

	range = DIV_ROUND_CLOSEST((uint32_t)range * 0xffff, dev_max_range);

	errno = t150_set_range(t150, range);
	if (errno)
		return errno;

	return count;
}

static ssize_t t150_show_range(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	uint16_t dev_max_range = 0;
	uint16_t r = 0;

	if (t150_refresh_for_sysfs(t150) < 0)
		return -EIO;

	if (t150_get_max_range(t150, &dev_max_range) < 0)
		return -EINVAL;

	t150_get_range(t150, &r);
	return sysfs_emit(buf, "%u\n", DIV_ROUND_CLOSEST((uint32_t)r * dev_max_range, 0xffff));
}

static ssize_t t150_store_ffb_intensity(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	uint16_t nforce;
	struct t150 *t150 = dev_get_drvdata(dev);
	int errno;

	if (kstrtou16(buf, 10, &nforce))
		return -EINVAL;

	errno = t150_set_gain(t150, nforce);
	if (errno)
		return errno;

	return count;
}

static ssize_t t150_show_ffb_intensity(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	uint16_t g = 0;

	if (t150_refresh_for_sysfs(t150) < 0)
		return -EIO;

	t150_get_gain(t150, &g);
	return sysfs_emit(buf, "%u\n", g);
}

static ssize_t t150_show_fw_version(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	unsigned long flags;
	uint16_t v = 0;

	if (t150_refresh_for_sysfs(t150) < 0)
		return -EIO;

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	v = t150->settings.firmware_version;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);

	return sysfs_emit(buf, "%u\n", v);
}

static ssize_t t150_store_reload_settings(struct device *dev, struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct t150 *t150 = dev_get_drvdata(dev);

	if (t150_refresh_settings_cached(t150, true) < 0)
		return -EIO;
	return count;
}

static ssize_t t150_show_max_range(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t max_range;
	struct t150 *t150 = dev_get_drvdata(dev);

	if (t150_get_max_range(t150, &max_range) < 0)
		return -EINVAL;

	return sysfs_emit(buf, "%u\n", max_range);
}

static ssize_t t150_show_settings_cache_age_ms(struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	unsigned long flags;
	unsigned long age_ms;
	bool cache_valid;
	unsigned long cache_updated_jiffies;

	t150_refresh_settings_cached(t150, false);

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	cache_valid = t150->settings.cache_valid;
	cache_updated_jiffies = t150->settings.cache_updated_jiffies;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);

	if (!cache_valid)
		return sysfs_emit(buf, "-1\n");

	age_ms = jiffies_to_msecs(jiffies - cache_updated_jiffies);
	return sysfs_emit(buf, "%lu\n", age_ms);
}

static ssize_t t150_show_supported_ff_effects(struct device *dev, struct device_attribute *attr,
					      char *buf)
{
	return sysfs_emit(buf,
			  "FF_GAIN,FF_AUTOCENTER,FF_PERIODIC,FF_SINE,FF_SAW_UP,FF_SAW_DOWN,FF_RUMBLE,FF_CONSTANT,FF_SPRING,FF_DAMPER\n");
}

static ssize_t t150_show_spring_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", t150->spring_level);
}

static ssize_t t150_store_spring_level(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	unsigned int value;

	if (kstrtouint(buf, 0, &value))
		return -EINVAL;
	if (value > 100)
		value = 100;

	t150->spring_level = value;
	t150_ff_resubmit_condition_effects(t150, FF_SPRING);
	return count;
}

static ssize_t t150_show_damper_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct t150 *t150 = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", t150->damper_level);
}

static ssize_t t150_store_damper_level(struct device *dev, struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct t150 *t150 = dev_get_drvdata(dev);
	unsigned int value;

	if (kstrtouint(buf, 0, &value))
		return -EINVAL;
	if (value > 100)
		value = 100;

	t150->damper_level = value;
	t150_ff_resubmit_condition_effects(t150, FF_DAMPER);
	return count;
}

static ssize_t t150_store_firmware_upgrade(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct t150 *t150 = dev_get_drvdata(dev);

	hid_warn(t150->hid_device,
		 "firmware upgrade invoked (%zu bytes) - not implemented\n", count);
	return -EOPNOTSUPP;
}
