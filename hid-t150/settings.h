#define SETTINGS_CACHE_TTL_MS 300

typedef uint8_t operation_t;
#define SET40_RETURN_FORCE		0x03
#define SET40_USE_RETURN_FORCE		0x04
#define SET40_RANGE			0x11

struct operation40 {
	uint8_t		code; /* MUST BE 0x40 */
	operation_t	operation;
	uint16_t	argument;
};

#define SET42_STOP_WHEEL_SETTINGS	0x00
#define SET42_START_WHEEL_SETTINGS	0x04
#define SET42_APPLY_WHEEL_SETTINGS	0x05

struct opertation42 {
	uint8_t		code; /* MUST BE 0x42 */
	operation_t	operation;
};

static int t150_settings_set40(struct t150 *t150, operation_t operation,
			       uint16_t argument);

static int t150_set_gain(struct t150 *t150, uint16_t gain);
static int t150_set_autocenter(struct t150 *t150, uint16_t autocenter_force);
static int t150_set_enable_autocenter(struct t150 *t150, bool enable);
static int t150_set_range(struct t150 *t150, uint16_t range);

static int t150_setup_task(struct t150 *t150);

static int t150_read_settings(struct t150 *t150);
static int t150_refresh_settings_cached(struct t150 *t150, bool force);
static int t150_refresh_for_sysfs(struct t150 *t150);
static void t150_settings_refresh_work(struct work_struct *work);
static void t150_schedule_settings_refresh(struct t150 *t150);
static void t150_cancel_settings_refresh(struct t150 *t150);

static int t150_get_gain(struct t150 *t150, uint16_t *gain);
static int t150_get_autocenter(struct t150 *t150, uint16_t *autocenter_force);
static int t150_get_enable_autocenter(struct t150 *t150, bool *enable);
static int t150_get_range(struct t150 *t150, uint16_t *range);
static int t150_get_max_range(struct t150 *t150, uint16_t *max_range);
