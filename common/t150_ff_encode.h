#ifndef T150_FF_ENCODE_H
#define T150_FF_ENCODE_H

/*
 * Force-feedback packet encoder for the FreeBSD ht150 path and host tests.
 *
 * Scaling matches hid-t150/forcefeedback.c. Linux hid-t150 does not
 * include this file; keep the two encoders in sync by hand. Do not
 * invent friction or inertia commit codes here.
 */

#include "t150_packets.h"

#if defined(_KERNEL) && defined(__FreeBSD__)
#include <sys/endian.h>
#include <sys/libkern.h>
#include <dev/evdev/input.h>
#define t150_htole16(v)	htole16(v)
#elif defined(__linux__) && defined(__KERNEL__)
#include <linux/input.h>
#include <linux/byteorder/generic.h>
#define t150_htole16(v)	cpu_to_le16(v)
#else
#include <string.h>
#include <stdint.h>
#if defined(__linux__) && defined(__has_include)
#if __has_include(<linux/input.h>)
#include <linux/input.h>
#endif
#endif
#ifndef FF_RUMBLE
/* Minimal Linux-shaped ff_effect for host encode tests. */
struct ff_envelope {
	uint16_t	attack_length;
	uint16_t	attack_level;
	uint16_t	fade_length;
	uint16_t	fade_level;
};
struct ff_constant_effect {
	int16_t			level;
	struct ff_envelope	envelope;
};
struct ff_ramp_effect {
	int16_t			start_level;
	int16_t			end_level;
	struct ff_envelope	envelope;
};
struct ff_periodic_effect {
	uint16_t		waveform;
	uint16_t		period;
	int16_t			magnitude;
	int16_t			offset;
	uint16_t		phase;
	struct ff_envelope	envelope;
	uint32_t		custom_len;
	int16_t			*custom_data;
};
struct ff_condition_effect {
	uint16_t	right_saturation;
	uint16_t	left_saturation;
	int16_t		right_coeff;
	int16_t		left_coeff;
	uint16_t	deadband;
	int16_t		center;
};
struct ff_rumble_effect {
	uint16_t	strong_magnitude;
	uint16_t	weak_magnitude;
};
struct ff_trigger {
	uint16_t	button;
	uint16_t	interval;
};
struct ff_replay {
	uint16_t	length;
	uint16_t	delay;
};
struct ff_effect {
	uint16_t		type;
	int16_t			id;
	uint16_t		direction;
	struct ff_trigger	trigger;
	struct ff_replay	replay;
	union {
		struct ff_constant_effect	constant;
		struct ff_ramp_effect		ramp;
		struct ff_periodic_effect	periodic;
		struct ff_condition_effect	condition[2];
		struct ff_rumble_effect		rumble;
	} u;
};
#define FF_RUMBLE	0x50
#define FF_PERIODIC	0x51
#define FF_CONSTANT	0x52
#define FF_SPRING	0x53
#define FF_DAMPER	0x55
#define FF_SINE		0x5a
#define FF_SAW_UP	0x5b
#define FF_SAW_DOWN	0x5c
#define FF_GAIN		0x60
#define FF_AUTOCENTER	0x61
#endif
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define t150_htole16(v)	(uint16_t)(v)
#else
static inline uint16_t t150_htole16(uint16_t v)
{
	return (uint16_t)((v << 8) | (v >> 8));
}
#endif
#endif

#ifndef T150_DIV_ROUND_CLOSEST
#define T150_DIV_ROUND_CLOSEST(x, d)	(((x) + ((d) / 2u)) / (d))
#endif

/* Same 16-bit sine as Linux fixp_sin16 (degrees, Q15). */
static const int32_t t150_sin32_table[91] = {
	0x00000000, 0x023be165, 0x04779632, 0x06b2f1d2, 0x08edc7b6, 0x0b27eb5c,
	0x0d61304d, 0x0f996a26, 0x11d06c96, 0x14060b67, 0x163a1a7d, 0x186c6ddd,
	0x1a9cd9ac, 0x1ccb3236, 0x1ef74bf2, 0x2120fb82, 0x234815ba, 0x256c6f9e,
	0x278dde6e, 0x29ac379f, 0x2bc750e8, 0x2ddf003f, 0x2ff31bdd, 0x32037a44,
	0x340ff241, 0x36185aee, 0x381c8bb5, 0x3a1c5c56, 0x3c17a4e7, 0x3e0e3ddb,
	0x3fffffff, 0x41ecc483, 0x43d464fa, 0x45b6bb5d, 0x4793a20f, 0x496af3e1,
	0x4b3c8c11, 0x4d084650, 0x4ecdfec6, 0x508d9210, 0x5246dd48, 0x53f9be04,
	0x55a6125a, 0x574bb8e5, 0x58ea90c2, 0x5a827999, 0x5c135399, 0x5d9cff82,
	0x5f1f5ea0, 0x609a52d1, 0x620dbe8a, 0x637984d3, 0x64dd894f, 0x6639b039,
	0x678dde6d, 0x68d9f963, 0x6a1de735, 0x6b598ea1, 0x6c8cd70a, 0x6db7a879,
	0x6ed9eba0, 0x6ff389de, 0x71046d3c, 0x720c8074, 0x730baeec, 0x7401e4bf,
	0x74ef0ebb, 0x75d31a5f, 0x76adf5e5, 0x777f903b, 0x7847d908, 0x7906c0af,
	0x79bc384c, 0x7a6831b8, 0x7b0a9f8c, 0x7ba3751c, 0x7c32a67c, 0x7cb82884,
	0x7d33f0c8, 0x7da5f5a3, 0x7e0e2e31, 0x7e6c924f, 0x7ec11aa3, 0x7f0bc095,
	0x7f4c7e52, 0x7f834ecf, 0x7fb02dc4, 0x7fd317b3, 0x7fec09e1, 0x7ffb025e,
	0x7fffffff
};

static inline int16_t t150_sin16_deg(int degrees)
{
	int32_t ret;
	int negative = 0;

	degrees %= 360;
	if (degrees < 0)
		degrees += 360;
	if (degrees > 180) {
		negative = 1;
		degrees -= 180;
	}
	if (degrees > 90)
		degrees = 180 - degrees;
	ret = t150_sin32_table[degrees];
	if (negative)
		ret = -ret;
	return (int16_t)(ret >> 16);
}

static inline void t150_ff_rewrite_rumble(struct ff_effect *effect)
{
	int magnitude;

	if (effect->type != FF_RUMBLE)
		return;

	magnitude = effect->u.rumble.strong_magnitude / 3
		  + effect->u.rumble.weak_magnitude / 6;

	effect->type = FF_PERIODIC;
	effect->direction = 16384;
	effect->u.periodic.waveform = FF_SINE;
	effect->u.periodic.period = 50;
	effect->u.periodic.magnitude = (uint16_t)magnitude;
	effect->u.periodic.offset = 0;
	effect->u.periodic.phase = 0;
	effect->u.periodic.envelope.attack_length = 0;
	effect->u.periodic.envelope.attack_level = 0;
	effect->u.periodic.envelope.fade_length = 0;
	effect->u.periodic.envelope.fade_level = 0;
}

static inline uint8_t t150_scale_envelope_level(uint16_t level)
{
	if (level > 0x7fff)
		level = 0x7fff;
	return (uint8_t)T150_DIV_ROUND_CLOSEST((uint32_t)level * 0xff, 0x7fff);
}

static inline uint8_t t150_scale_phase(uint16_t phase)
{
	uint32_t p = phase;

	if (p >= 36000)
		p %= 36000;
	return (uint8_t)T150_DIV_ROUND_CLOSEST(p * 0xff, 36000);
}

static inline int8_t t150_scale_coeff(int16_t coeff, uint8_t level)
{
	int32_t v = coeff / 0x147;

	v = v * level / 100;
	if (v > 100)
		v = 100;
	if (v < -100)
		v = -100;
	return (int8_t)v;
}

static inline uint8_t t150_scale_sat(uint16_t sat, uint16_t divisor, uint8_t level)
{
	uint32_t v;

	if (!divisor)
		return 0;
	v = sat / divisor;
	v = v * level / 100;
	if (v > 0xff)
		v = 0xff;
	return (uint8_t)v;
}

static inline void t150_ff_prepare_first(struct t150_ff_first *ff_first,
					 const struct ff_effect *effect)
{
	const struct ff_envelope *ff_envelope = NULL;

	memset(ff_first, 0, sizeof(*ff_first));

	switch (effect->type) {
	case FF_CONSTANT:
		ff_envelope = &effect->u.constant.envelope;
		ff_first->f0 = T150_FF_FIRST_CODE_CONSTANT;
		break;
	case FF_PERIODIC:
		ff_envelope = &effect->u.periodic.envelope;
		ff_first->f0 = T150_FF_FIRST_CODE_PERIODIC;
		break;
	case FF_DAMPER:
	case FF_SPRING:
		ff_first->f0 = T150_FF_FIRST_CODE_CONDITION;
		break;
	default:
		break;
	}

	ff_first->pk_id0 = (uint8_t)(effect->id * 0x1c + 0x1c);
	ff_first->f1 = 0;
	ff_first->f2 = 0x46;
	ff_first->f3 = 0x54;

	if (ff_envelope) {
		ff_first->attack_length = t150_htole16(ff_envelope->attack_length);
		ff_first->attack_level = t150_scale_envelope_level(ff_envelope->attack_level);
		ff_first->fade_length = t150_htole16(ff_envelope->fade_length);
		ff_first->fade_level = t150_scale_envelope_level(ff_envelope->fade_level);
	}
}

static inline void t150_ff_prepare_update(struct t150_ff_update *ff_update,
					  const struct ff_effect *effect,
					  uint8_t spring_level,
					  uint8_t damper_level)
{
	int32_t level = 0;
	uint8_t cond_level;
	int degrees;

	memset(ff_update, 0, sizeof(*ff_update));
	ff_update->pk_id1 = (uint8_t)(effect->id * 0x1c + 0x0e);
	ff_update->f1 = 0x00;

	switch (effect->type) {
	case FF_PERIODIC:
		ff_update->effect_class = T150_FF_UPDATE_CODE_PERIODIC;
		ff_update->effect.periodic.magnitude =
			(int8_t)t150_word_high(effect->u.periodic.magnitude);
		ff_update->effect.periodic.offset =
			(int8_t)t150_word_high(effect->u.periodic.offset);
		ff_update->effect.periodic.phase =
			t150_scale_phase(effect->u.periodic.phase);
		ff_update->effect.periodic.period =
			t150_htole16(effect->u.periodic.period);
		break;
	case FF_CONSTANT:
		ff_update->effect_class = T150_FF_UPDATE_CODE_CONSTANT;
		degrees = effect->direction / (0xFFFF / 360);
		level = effect->u.constant.level * t150_sin16_deg(degrees);
		level >>= 15;
		ff_update->effect.constant.level = (int8_t)(level / 0x01ff);
		break;
	case FF_SPRING:
		cond_level = spring_level;
		ff_update->effect_class = T150_FF_UPDATE_CODE_CONDITION;
		ff_update->effect.condition.right_coeff =
			t150_scale_coeff(effect->u.condition[0].right_coeff, cond_level);
		ff_update->effect.condition.left_coeff =
			t150_scale_coeff(effect->u.condition[0].left_coeff, cond_level);
		ff_update->effect.condition.center = t150_htole16(
			(uint16_t)(effect->u.condition[0].center / (0x7fff / 0x01f4)));
		ff_update->effect.condition.deadband = t150_htole16(
			(uint16_t)(effect->u.condition[0].deadband / (0xffff / 0x03e8)));
		ff_update->effect.condition.right_sat =
			t150_scale_sat(effect->u.condition[0].right_saturation, 0x030c, cond_level);
		ff_update->effect.condition.left_sat =
			t150_scale_sat(effect->u.condition[0].left_saturation, 0x030c, cond_level);
		break;
	case FF_DAMPER:
		cond_level = damper_level;
		ff_update->effect_class = T150_FF_UPDATE_CODE_CONDITION;
		ff_update->effect.condition.right_coeff =
			t150_scale_coeff(effect->u.condition[0].right_coeff, cond_level);
		ff_update->effect.condition.left_coeff =
			t150_scale_coeff(effect->u.condition[0].left_coeff, cond_level);
		ff_update->effect.condition.center = t150_htole16(
			(uint16_t)(effect->u.condition[0].center / (0x7fff / 0x01f4)));
		ff_update->effect.condition.deadband = t150_htole16(
			(uint16_t)(effect->u.condition[0].deadband / (0xffff / 0x03e8)));
		ff_update->effect.condition.right_sat =
			t150_scale_sat(effect->u.condition[0].right_saturation, 0x028f, cond_level);
		ff_update->effect.condition.left_sat =
			t150_scale_sat(effect->u.condition[0].left_saturation, 0x028f, cond_level);
		break;
	default:
		ff_update->effect_class = T150_FF_UPDATE_CODE_PERIODIC;
		break;
	}
}

static inline void t150_ff_prepare_commit(struct t150_ff_commit *ff_commit,
					  const struct ff_effect *effect)
{
	uint16_t delay;

	memset(ff_commit, 0, sizeof(*ff_commit));
	ff_commit->f0 = 0x01;
	ff_commit->id = (uint8_t)effect->id;
	if (effect->replay.length)
		ff_commit->length = t150_htole16(effect->replay.length);
	else
		ff_commit->length = t150_htole16(0xffff);
	ff_commit->f1 = 0;
	ff_commit->f2 = 0;
	ff_commit->pk_id1 = (uint8_t)(effect->id * 0x1c + 0x0e);
	ff_commit->f3 = 0;
	ff_commit->pk_id0 = (uint8_t)(effect->id * 0x1c + 0x1c);
	ff_commit->f4 = 0;
	delay = effect->replay.delay;
	if (delay > 0xff)
		delay = 0xff;
	ff_commit->delay = (uint8_t)delay;
	ff_commit->f5 = 0;

	switch (effect->type) {
	case FF_PERIODIC:
		switch (effect->u.periodic.waveform) {
		case FF_SAW_UP:
			ff_commit->effect_type = t150_htole16(T150_FF_COMMIT_CODE_SAW_UP);
			break;
		case FF_SAW_DOWN:
			ff_commit->effect_type = t150_htole16(T150_FF_COMMIT_CODE_SAW_DOWN);
			break;
		case FF_SINE:
		default:
			ff_commit->effect_type = t150_htole16(T150_FF_COMMIT_CODE_SINE);
			break;
		}
		break;
	case FF_CONSTANT:
		ff_commit->effect_type = t150_htole16(T150_FF_COMMIT_CODE_CONSTANT);
		break;
	case FF_SPRING:
		ff_commit->effect_type = t150_htole16(T150_FF_COMMIT_CODE_SPRING);
		break;
	case FF_DAMPER:
		ff_commit->effect_type = t150_htole16(T150_FF_COMMIT_CODE_DAMPER);
		break;
	default:
		break;
	}
}

static inline void t150_ff_prepare_play(struct t150_ff_change_effect_status *pkt,
					int effect_id, int times)
{
	pkt->f0 = T150_FF_PLAY_CODE;
	pkt->id = (uint8_t)effect_id;
	pkt->mode = times ? 0x41 : 0x00;
	if (times) {
		if (times > 0xff)
			times = 0xff;
		pkt->times = (uint8_t)times;
	} else {
		pkt->times = 0x01;
	}
}

static inline void t150_ff_prepare_gain(struct t150_ff_change_gain *pkt, uint16_t gain)
{
	pkt->f0 = T150_FF_GAIN_CODE;
	pkt->gain = t150_htole16(gain);
}

static inline void t150_prepare_set40(struct t150_operation40 *pkt,
				      uint8_t operation, uint16_t argument)
{
	pkt->code = SET40_CODE;
	pkt->operation = operation;
	pkt->argument = t150_htole16(argument);
}

static inline uint16_t t150_range_to_wire(uint16_t degrees, uint16_t max_degrees)
{
	if (degrees < T150_RANGE_MIN)
		degrees = T150_RANGE_MIN;
	if (degrees > max_degrees)
		degrees = max_degrees;
	return (uint16_t)T150_DIV_ROUND_CLOSEST((uint32_t)degrees * 0xffff, max_degrees);
}

static inline uint16_t t150_range_from_wire(uint16_t wire, uint16_t max_degrees)
{
	return (uint16_t)T150_DIV_ROUND_CLOSEST((uint32_t)wire * max_degrees, 0xffff);
}

static inline uint16_t t150_autocenter_to_percent(uint16_t autocenter_force)
{
	return (uint16_t)T150_DIV_ROUND_CLOSEST((uint32_t)autocenter_force * 100, 0xffff);
}

#endif /* T150_FF_ENCODE_H */
