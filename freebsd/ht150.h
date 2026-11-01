#ifndef _HT150_H_
#define _HT150_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <dev/hid/hidmap.h>
#include <dev/evdev/input.h>

#include "t150_packets.h"

#define	HT150_NAME		"ht150"
#define	HT150_DESC		"Thrustmaster T150/TMX"

struct ht150_softc {
	struct hidmap		hm;	/* first: hidmap callbacks use device_get_softc */
	device_t		dev;
	struct sx		lock;
	void			*usb_buf;
	size_t			usb_buf_size;
	int			gone;

	uint16_t		product;
	uint16_t		max_range;
	uint16_t		range;		/* degrees */
	uint16_t		gain;
	uint16_t		autocenter;
	int			enable_autocenter;
	uint8_t			spring_level;
	uint8_t			damper_level;
	uint8_t			firmware_version;
	int			cache_valid;

	struct ff_effect	effects[T150_FF_MAX_EFFECTS];
	int			effect_used[T150_FF_MAX_EFFECTS];

	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

int	ht150_send(struct ht150_softc *sc, const void *data, size_t len);
int	ht150_set_gain(struct ht150_softc *sc, uint16_t gain);
int	ht150_set_autocenter(struct ht150_softc *sc, uint16_t force);
int	ht150_set_enable_autocenter(struct ht150_softc *sc, int enable);
int	ht150_set_range_deg(struct ht150_softc *sc, uint16_t degrees);
int	ht150_read_settings(struct ht150_softc *sc);
struct t150_preset;

extern char	ht150_default_preset[];

int	ht150_setup(struct ht150_softc *sc);
int	ht150_apply_preset(struct ht150_softc *sc, const struct t150_preset *preset);
const struct t150_preset *ht150_matched_preset(struct ht150_softc *sc);
int	ht150_ff_upload(struct ht150_softc *sc, struct ff_effect *effect);
int	ht150_ff_play(struct ht150_softc *sc, int effect_id, int times);
int	ht150_ff_test(struct ht150_softc *sc, int enable);

#endif /* _HT150_H_ */
