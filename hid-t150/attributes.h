/********************************************************************
 *			             SYSF STUFF
 *
 *        This section contains the handlers for the
 *          attributes created in the sysfs when a
 *             wheel is plugged into the host
 *******************************************************************/

static inline int t150_init_attributes(struct t150 *t150);
static inline void t150_free_attributes(struct t150 *t150);

static ssize_t t150_store_return_force(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t t150_show_return_force(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_store_simulate_return_force(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t t150_show_simulate_return_force(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_store_range(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t t150_show_range(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_store_ffb_intensity(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t t150_show_ffb_intensity(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_show_fw_version(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_show_max_range(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_show_settings_cache_age_ms(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_show_supported_ff_effects(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_store_spring_level(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t t150_show_spring_level(struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t t150_store_damper_level(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t t150_show_damper_level(struct device *dev, struct device_attribute *attr, char *buf);

static ssize_t t150_store_reload_settings(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);
static ssize_t t150_store_firmware_upgrade(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count);

/* 0664: kernel forbids world-writable sysfs (VERIFY_OCTAL_PERMISSIONS).
 * udev chmod 0666 after bind so Oversteer can write as the logged-in user. */
static DEVICE_ATTR(autocenter, 0664, t150_show_return_force, t150_store_return_force);
static DEVICE_ATTR(enable_autocenter, 0664, t150_show_simulate_return_force, t150_store_simulate_return_force);
static DEVICE_ATTR(range, 0664, t150_show_range, t150_store_range);
static DEVICE_ATTR(gain, 0664, t150_show_ffb_intensity, t150_store_ffb_intensity);
static DEVICE_ATTR(firmware_version, 0444, t150_show_fw_version, 0);
static DEVICE_ATTR(max_range, 0444, t150_show_max_range, 0);
static DEVICE_ATTR(settings_cache_age_ms, 0444, t150_show_settings_cache_age_ms, 0);
static DEVICE_ATTR(supported_ff_effects, 0444, t150_show_supported_ff_effects, 0);
static DEVICE_ATTR(spring_level, 0664, t150_show_spring_level, t150_store_spring_level);
static DEVICE_ATTR(damper_level, 0664, t150_show_damper_level, t150_store_damper_level);
static DEVICE_ATTR(reload_settings, 0200, NULL, t150_store_reload_settings);
static DEVICE_ATTR(firmware_upgrade, 0200, NULL, t150_store_firmware_upgrade);
