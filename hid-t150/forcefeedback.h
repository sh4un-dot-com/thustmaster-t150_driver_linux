/* Packet layout and commit codes: keep in sync with common/t150_packets.h. */
#define T150_FF_BLIND_COMPUTE_EFFECT		false
#define T150_FF_BLIND_UPLOAD			false

/* pk_id0/pk_id1 are 8-bit (id * 0x1c + 0x1c must fit). Linux ids 0..8. */
#define T150_FF_MAX_EFFECTS			9

#define T150_FF_FIRST_CODE_CONSTANT		0x02
#define T150_FF_FIRST_CODE_PERIODIC		0x02
#define T150_FF_FIRST_CODE_CONDITION		0x05

#define T150_FF_UPDATE_CODE_CONSTANT		0x03
#define T150_FF_UPDATE_CODE_PERIODIC		0x04
#define T150_FF_UPDATE_CODE_CONDITION		0x05

#define T150_FF_COMMIT_CODE_CONSTANT		0x4000
#define T150_FF_COMMIT_CODE_SINE		0x4022
#define T150_FF_COMMIT_CODE_SAW_UP		0x4023
#define T150_FF_COMMIT_CODE_SAW_DOWN		0x4024
#define T150_FF_COMMIT_CODE_SPRING		0x4040
#define T150_FF_COMMIT_CODE_DAMPER		0x4041

struct __packed ff_periodic {
	int8_t		magnitude;
	int8_t		offset;
	uint8_t		phase;
	uint16_t	period;
};

struct __packed ff_constant {
	int8_t		level;
};

struct __packed ff_condition {
	int8_t		right_coeff;
	int8_t		left_coeff;
	int16_t		center;
	int16_t		deadband;
	uint8_t		right_sat;
	uint8_t		left_sat;
};

struct __packed ff_first {
	uint8_t		f0;
	uint8_t		pk_id0;
	uint8_t		f1;
	uint16_t	attack_length;
	uint8_t		attack_level;
	uint16_t	fade_length;
	uint8_t		fade_level;
	uint8_t		f2;
	uint8_t		f3;
};

struct __packed ff_update {
	uint8_t		effect_class;
	uint8_t		pk_id1;
	uint8_t		f1;

	union {
		struct ff_periodic periodic;
		struct ff_constant constant;
		struct ff_condition condition;
	} effect;
};

struct __packed ff_commit {
	uint8_t		f0;
	uint8_t		id;
	uint16_t	effect_type;
	uint16_t	length;
	uint16_t	f1;
	uint8_t		f2;
	uint8_t		pk_id1;
	uint8_t		f3;
	uint8_t		pk_id0;
	uint8_t		f4;
	uint8_t		delay;
	uint8_t		f5;
};

struct __packed ff_change_effect_status {
	uint8_t f0;
	uint8_t id;
	uint8_t mode;
	uint8_t times;
};

struct __packed ff_change_gain {
	uint8_t f0;
	uint16_t gain;
};

union __packed ff_change {
	struct ff_change_effect_status effect;
	struct ff_change_gain gain;
};

static int t150_init_ffb(struct t150 *t150);
static void t150_free_ffb(struct t150 *t150);

static int t150_ff_upload(struct input_dev *dev, struct ff_effect *effect, struct ff_effect *old);
static int t150_ff_erase(struct input_dev *dev, int effect_id);
static int t150_ff_play(struct input_dev *dev, int effect_id, int value);
static void t150_ff_set_gain(struct input_dev *dev, uint16_t gain);
static void t150_ff_set_autocenter(struct input_dev *dev, uint16_t value);
static void t150_ff_work(struct work_struct *work);
static void t150_ff_resubmit_condition_effects(struct t150 *t150, uint16_t type);

static uint8_t t150_ffb_effects_length = 10;
static const int16_t t150_ffb_effects[] = {
	FF_GAIN,
	FF_AUTOCENTER,

	FF_PERIODIC,
	FF_SINE,
	FF_SAW_UP,
	FF_SAW_DOWN,
	FF_RUMBLE,

	FF_CONSTANT,

	FF_SPRING,
	FF_DAMPER
};
