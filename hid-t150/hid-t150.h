#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/input.h>

#define USB_THRUSTMASTER_VENDOR_ID	0x044f
#define USB_T150_PRODUCT_ID		0xb677
#define USB_TMX_PRODUCT_ID		0xb67f

#define T150_USB_TIMEOUT		USB_CTRL_SET_TIMEOUT
#define T150_USB_BUF_SIZE		64

#define T150_RANGE_MIN			270
#define T150_RANGE_T150_MAX		1080
#define T150_RANGE_TMX_MAX		900

#define T150_PKT_INPUT_OPEN		0x0442
#define T150_PKT_INPUT_WHAT		0x0542
#define T150_PKT_INPUT_CLOSE		0x0042

struct joy_state_packet;
struct ff_first;
struct ff_second;
struct ff_third;
union ff_change;

struct t150_ff_effect {
	struct ff_effect effect;
	struct ff_effect old;
	bool has_old;
	bool upload_pending;
	bool play_pending;
	int play_value;
};

struct t150 {
	struct usb_device *usb_device;
	struct hid_device *hid_device;

	int pipe_in;
	uint8_t bInterval_in;

	int pipe_out;
	uint8_t bInterval_out;

	char dev_path[128];
	struct input_dev *joystick;

	struct mutex lock;
	void *usb_buf;
	size_t usb_buf_size;
	bool gone;

	struct {
		spinlock_t access_lock;

		uint16_t autocenter_force;
		bool autocenter_enabled;
		uint16_t range;
		uint16_t gain;

		uint8_t firmware_version;
		bool cache_valid;
		unsigned long cache_updated_jiffies;
	} settings;

	struct work_struct settings_refresh_work;

	spinlock_t ff_lock;
	struct work_struct ff_work;
	struct t150_ff_effect *ff_effects;
	uint16_t pending_gain;
	uint16_t pending_autocenter;
	bool gain_pending;
	bool autocenter_pending;

	uint8_t spring_level;
	uint8_t damper_level;
};

static inline uint16_t make_word(const uint8_t low, const uint8_t high)
{
	return ((uint16_t)low | ((uint16_t)high << 8));
}

static inline uint8_t word_high(const uint16_t word)
{
	return word >> 8;
}

static inline uint8_t word_low(const uint16_t word)
{
	return word;
}

static inline void printP(uint8_t const *const bytes, const size_t length)
{
	int i;
	char printstr[202] = "t150: ";

	if (length > 64)
		return;

	for (i = 0; i < length; i++)
		sprintf(printstr + (6 + i * 3), "%02hhX ", bytes[i]);

	sprintf(printstr + (6 + length * 3), "\n");

	printk("%s", printstr);
}

static int t150_send_int(struct t150 *t150, const void *data, size_t len);
static void t150_ff_schedule(struct t150 *t150);
