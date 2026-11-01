#ifndef T150_PRESETS_H
#define T150_PRESETS_H

/*
 * Wheel-feel presets for T150/TMX. Shared by hid-t150, ht150, and host tests.
 *
 * The hardware only exposes rotation, global gain, hardware autocenter, and
 * scales for game-uploaded FF_SPRING / FF_DAMPER. Range is the main lock-to-
 * lock feel. Hardware autocenter is for games that do not center the wheel.
 * spring_level / damper_level do not add extra effects; they scale what the
 * game sends.
 *
 * These are starting points for a belt-driven T150, not per-game FFB files.
 * Default plug-in preset is "sim".
 */

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/types.h>
#elif defined(__FreeBSD__)
#include <sys/types.h>
#else
#include <stdint.h>
#endif

#ifndef T150_RANGE_MIN
#define T150_RANGE_MIN		270
#endif

/* Percent 0..100 -> 0..65535, rounded. */
#define T150_GAIN_PCT(p)	((uint16_t)(((uint32_t)(p) * 65535u + 50u) / 100u))

struct t150_preset {
	const char	*name;
	const char	*aliases;	/* space-separated, or empty */
	const char	*description;
	uint16_t	range_deg;	/* clamped to the device maximum */
	uint16_t	gain;		/* 0..65535 */
	uint16_t	autocenter;	/* 0..65535, desktop / hardware spring */
	uint8_t		enable_autocenter; /* keep hardware center while a game is open */
	uint8_t		spring_level;	/* 0..100, scale of FF_SPRING */
	uint8_t		damper_level;	/* 0..100, scale of FF_DAMPER */
};

/*
 * Canonical names are the first field. Aliases are accepted on write.
 * Keep "sim" first: it is the default.
 */
static const struct t150_preset t150_presets[] = {
	{
		"sim", "simracing sim-racing racing_sim",
		"Sim racing (ACC, iRacing, AC, rF2). Full rotation, game centering.",
		1080, T150_GAIN_PCT(100), T150_GAIN_PCT(50), 0, 100, 100
	},
	{
		"arcade", "arcade_racer kart",
		"Arcade racers. 270°, hardware snap-back, light damper.",
		270, T150_GAIN_PCT(100), T150_GAIN_PCT(40), 1, 100, 30
	},
	{
		"street", "street_racing",
		"Street / open world (NFS, Forza Horizon). 540°, moderate weight.",
		540, T150_GAIN_PCT(90), T150_GAIN_PCT(30), 0, 60, 45
	},
	{
		"offroad", "off-road off_road",
		"Off-road. 540°, full gain, heavier damper for chatter.",
		540, T150_GAIN_PCT(100), T150_GAIN_PCT(25), 0, 50, 75
	},
	{
		"rally", "wrc",
		"Rally (Dirt Rally, WRC). 540°, light damper so snaps stay readable.",
		540, T150_GAIN_PCT(90), T150_GAIN_PCT(10), 0, 25, 35
	},
	{
		"drift", "drifting",
		"Drifting. 360°, no centering, light damper for fast countersteer.",
		360, T150_GAIN_PCT(80), 0, 0, 0, 20
	},
	{
		"truck", "truck_sim ets2 ats",
		"Truck sims (ETS2, ATS). Full rotation, heavier spring/damper.",
		1080, T150_GAIN_PCT(70), T150_GAIN_PCT(30), 0, 70, 80
	},
};

#define T150_PRESET_COUNT	(sizeof(t150_presets) / sizeof(t150_presets[0]))
#define T150_PRESET_DEFAULT	(&t150_presets[0])

static inline int t150_preset_tolower(unsigned char c)
{
	if (c >= 'A' && c <= 'Z')
		return c + 32;
	return c;
}

static inline int t150_preset_eq_len(const char *canon, const char *s, int slen)
{
	int i;

	for (i = 0; i < slen; i++) {
		if (!canon[i])
			return 0;
		if (t150_preset_tolower((unsigned char)canon[i]) !=
		    t150_preset_tolower((unsigned char)s[i]))
			return 0;
	}
	return canon[slen] == '\0';
}

static inline void t150_preset_trim(const char *name, const char **start, int *len)
{
	const char *s = name;
	const char *e;

	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
		s++;
	e = s;
	while (*e && *e != '\n' && *e != '\r')
		e++;
	while (e > s && (e[-1] == ' ' || e[-1] == '\t'))
		e--;
	*start = s;
	*len = (int)(e - s);
}

static inline int t150_preset_token_match(const char *aliases, const char *s, int slen)
{
	const char *p = aliases;
	const char *tok;
	int toklen, i;

	if (!p)
		return 0;

	while (*p) {
		while (*p == ' ' || *p == '\t')
			p++;
		tok = p;
		while (*p && *p != ' ' && *p != '\t')
			p++;
		toklen = (int)(p - tok);
		if (toklen != slen)
			continue;
		for (i = 0; i < slen; i++) {
			if (t150_preset_tolower((unsigned char)tok[i]) !=
			    t150_preset_tolower((unsigned char)s[i]))
				break;
		}
		if (i == slen)
			return 1;
	}
	return 0;
}

static inline const struct t150_preset *t150_preset_by_name(const char *name)
{
	const char *s;
	int slen;
	unsigned int i;

	if (!name)
		return 0;

	t150_preset_trim(name, &s, &slen);
	if (slen <= 0)
		return 0;

	for (i = 0; i < T150_PRESET_COUNT; i++) {
		if (t150_preset_eq_len(t150_presets[i].name, s, slen))
			return &t150_presets[i];
		if (t150_preset_token_match(t150_presets[i].aliases, s, slen))
			return &t150_presets[i];
	}
	return 0;
}

static inline uint16_t t150_preset_range_deg(const struct t150_preset *p,
					     uint16_t max_range)
{
	uint16_t r = p->range_deg;

	if (r < T150_RANGE_MIN)
		r = T150_RANGE_MIN;
	if (r > max_range)
		r = max_range;
	return r;
}

static inline const struct t150_preset *
t150_preset_lookup_values(uint16_t range_deg, uint16_t gain, uint16_t autocenter,
			  int enable_autocenter, uint8_t spring, uint8_t damper,
			  uint16_t max_range)
{
	unsigned int i;

	for (i = 0; i < T150_PRESET_COUNT; i++) {
		if (t150_preset_range_deg(&t150_presets[i], max_range) == range_deg &&
		    t150_presets[i].gain == gain &&
		    t150_presets[i].autocenter == autocenter &&
		    !!t150_presets[i].enable_autocenter == !!enable_autocenter &&
		    t150_presets[i].spring_level == spring &&
		    t150_presets[i].damper_level == damper)
			return &t150_presets[i];
	}
	return 0;
}

#endif /* T150_PRESETS_H */
