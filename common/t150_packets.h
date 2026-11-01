#ifndef T150_PACKETS_H
#define T150_PACKETS_H

/*
 * T150/TMX USB protocol. Keep in sync with hid-t150/forcefeedback.h,
 * hid-t150/settings.h, and hid-t150/hid-t150.h.
 *
 * All multi-byte fields on the wire are little-endian.
 */

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/types.h>
#define T150_PACKED __attribute__((packed))
#elif defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/cdefs.h>
#define T150_PACKED __packed
#else
#include <stdint.h>
#define T150_PACKED __attribute__((packed))
#endif

#ifndef USB_THRUSTMASTER_VENDOR_ID
#define USB_THRUSTMASTER_VENDOR_ID	0x044f
#endif
#ifndef USB_T150_PRODUCT_ID
#define USB_T150_PRODUCT_ID		0xb677
#endif
#ifndef USB_TMX_PRODUCT_ID
#define USB_TMX_PRODUCT_ID		0xb67f
#endif
#ifndef USB_THRUSTMASTER_BOOT_ID
#define USB_THRUSTMASTER_BOOT_ID	0xb65d
#endif

#define T150_RANGE_MIN			270
#define T150_RANGE_T150_MAX		1080
#define T150_RANGE_TMX_MAX		900

#define T150_PKT_INPUT_OPEN		0x0442
#define T150_PKT_INPUT_WHAT		0x0542
#define T150_PKT_INPUT_CLOSE		0x0042

#define T150_USB_BUF_SIZE		64
#define T150_SETTINGS_GET_REQUEST	86
#define T150_SETTINGS_GET_TYPE		0xc1
#define T150_INIT_GET_REQUEST		73
#define T150_INIT_SET_REQUEST		83
#define T150_INIT_GET_TYPE		0xc1
#define T150_INIT_SET_TYPE		0x41

/* pk_id0/pk_id1 are 8-bit (id * 0x1c + 0x1c must fit). Host ids 0..8. */
#define T150_FF_MAX_EFFECTS		9

#define T150_FF_FIRST_CODE_CONSTANT	0x02
#define T150_FF_FIRST_CODE_PERIODIC	0x02
#define T150_FF_FIRST_CODE_CONDITION	0x05

#define T150_FF_UPDATE_CODE_CONSTANT	0x03
#define T150_FF_UPDATE_CODE_PERIODIC	0x04
#define T150_FF_UPDATE_CODE_CONDITION	0x05

#define T150_FF_COMMIT_CODE_CONSTANT	0x4000
#define T150_FF_COMMIT_CODE_SINE	0x4022
#define T150_FF_COMMIT_CODE_SAW_UP	0x4023
#define T150_FF_COMMIT_CODE_SAW_DOWN	0x4024
#define T150_FF_COMMIT_CODE_SPRING	0x4040
#define T150_FF_COMMIT_CODE_DAMPER	0x4041

#define T150_FF_PLAY_CODE		0x41
#define T150_FF_GAIN_CODE		0x43

#define SET40_CODE			0x40
#define SET40_RETURN_FORCE		0x03
#define SET40_USE_RETURN_FORCE		0x04
#define SET40_RANGE			0x11

#define SET42_STOP_WHEEL_SETTINGS	0x00
#define SET42_START_WHEEL_SETTINGS	0x04
#define SET42_APPLY_WHEEL_SETTINGS	0x05

#define STATE_PACKET_INPUT		0x07

struct T150_PACKED t150_ff_periodic {
	int8_t		magnitude;
	int8_t		offset;
	uint8_t		phase;
	uint16_t	period;
};

struct T150_PACKED t150_ff_constant {
	int8_t		level;
};

struct T150_PACKED t150_ff_condition {
	int8_t		right_coeff;
	int8_t		left_coeff;
	int16_t		center;
	int16_t		deadband;
	uint8_t		right_sat;
	uint8_t		left_sat;
};

struct T150_PACKED t150_ff_first {
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

struct T150_PACKED t150_ff_update {
	uint8_t		effect_class;
	uint8_t		pk_id1;
	uint8_t		f1;
	union {
		struct t150_ff_periodic periodic;
		struct t150_ff_constant constant;
		struct t150_ff_condition condition;
	} effect;
};

/* Packed wire sizes: first 11, update 11, commit 15, play 4, gain 3. */
struct T150_PACKED t150_ff_commit {
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

struct T150_PACKED t150_ff_change_effect_status {
	uint8_t		f0;
	uint8_t		id;
	uint8_t		mode;
	uint8_t		times;
};

struct T150_PACKED t150_ff_change_gain {
	uint8_t		f0;
	uint16_t	gain;
};

struct T150_PACKED t150_operation40 {
	uint8_t		code;
	uint8_t		operation;
	uint16_t	argument;
};

struct T150_PACKED t150_operation42 {
	uint8_t		code;
	uint8_t		operation;
};

static inline uint16_t t150_make_word(uint8_t low, uint8_t high)
{
	return ((uint16_t)low | ((uint16_t)high << 8));
}

static inline uint8_t t150_word_high(uint16_t word)
{
	return (uint8_t)(word >> 8);
}

static inline uint8_t t150_word_low(uint16_t word)
{
	return (uint8_t)word;
}

#endif /* T150_PACKETS_H */
