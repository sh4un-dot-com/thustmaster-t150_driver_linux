/* Host-side checks for T150 packet sizes and FFB encode helpers. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "t150_ff_encode.h"
#include "t150_presets.h"

int
main(void)
{
	struct t150_ff_first first;
	struct t150_ff_update update;
	struct t150_ff_commit commit;
	struct t150_ff_change_effect_status play;
	struct t150_ff_change_gain gain;
	struct t150_operation40 op40;
	struct ff_effect effect;

	assert(sizeof(struct t150_ff_first) == 11);
	assert(sizeof(struct t150_ff_update) == 11);
	assert(sizeof(struct t150_ff_commit) == 15);
	assert(sizeof(struct t150_ff_change_effect_status) == 4);
	assert(sizeof(struct t150_ff_change_gain) == 3);
	assert(sizeof(struct t150_operation40) == 4);

	memset(&effect, 0, sizeof(effect));
	effect.type = FF_PERIODIC;
	effect.id = 0;
	effect.u.periodic.waveform = FF_SINE;
	effect.u.periodic.period = 50;
	effect.u.periodic.magnitude = 0x7fff;

	t150_ff_prepare_first(&first, &effect);
	assert(first.f0 == T150_FF_FIRST_CODE_PERIODIC);
	assert(first.pk_id0 == 0x1c);
	assert(first.f2 == 0x46);
	assert(first.f3 == 0x54);

	t150_ff_prepare_update(&update, &effect, 100, 100);
	assert(update.effect_class == T150_FF_UPDATE_CODE_PERIODIC);
	assert(update.pk_id1 == 0x0e);
	assert((uint8_t)update.effect.periodic.magnitude == 0x7f);

	t150_ff_prepare_commit(&commit, &effect);
	assert(commit.f0 == 0x01);
	assert(commit.id == 0);
	assert(commit.effect_type == t150_htole16(T150_FF_COMMIT_CODE_SINE));
	assert(commit.length == t150_htole16(0xffff));

	t150_ff_prepare_play(&play, 0, 1);
	assert(play.f0 == T150_FF_PLAY_CODE);
	assert(play.mode == 0x41);
	assert(play.times == 1);

	t150_ff_prepare_gain(&gain, 0xbffe);
	assert(gain.f0 == T150_FF_GAIN_CODE);

	t150_prepare_set40(&op40, SET40_RANGE, t150_range_to_wire(1080, 1080));
	assert(op40.code == SET40_CODE);
	assert(op40.operation == SET40_RANGE);

	memset(&effect, 0, sizeof(effect));
	effect.type = FF_CONSTANT;
	effect.id = 1;
	effect.direction = 16384; /* 90 degrees, full east */
	effect.u.constant.level = 0x7fff;
	t150_ff_prepare_update(&update, &effect, 100, 100);
	assert(update.effect_class == T150_FF_UPDATE_CODE_CONSTANT);
	/* sin16(90)=32767; 0x7fff*32767 >> 15 = 32766; 32766/0x1ff = 64 */
	assert(update.effect.constant.level == 64);

	assert(t150_range_to_wire(1080, 1080) == 0xffff);
	assert(t150_range_from_wire(0xffff, 1080) == 1080);
	assert(t150_autocenter_to_percent(0xffff) == 100);

	assert(t150_preset_by_name("sim") == T150_PRESET_DEFAULT);
	assert(t150_preset_by_name("SIM\n") == T150_PRESET_DEFAULT);
	assert(t150_preset_by_name("simracing") == T150_PRESET_DEFAULT);
	assert(t150_preset_by_name("drift") != NULL);
	assert(strcmp(t150_preset_by_name("drifting")->name, "drift") == 0);
	assert(t150_preset_by_name("off-road") != NULL);
	assert(t150_preset_by_name("nope") == NULL);
	assert(t150_preset_range_deg(t150_preset_by_name("sim"), 900) == 900);
	assert(t150_preset_range_deg(t150_preset_by_name("arcade"), 1080) == 270);
	assert(t150_preset_by_name("drift")->enable_autocenter == 0);
	assert(t150_preset_by_name("arcade")->enable_autocenter == 1);
	assert(t150_preset_lookup_values(
		   t150_preset_range_deg(T150_PRESET_DEFAULT, 1080),
		   T150_PRESET_DEFAULT->gain,
		   T150_PRESET_DEFAULT->autocenter,
		   T150_PRESET_DEFAULT->enable_autocenter,
		   T150_PRESET_DEFAULT->spring_level,
		   T150_PRESET_DEFAULT->damper_level,
		   1080) == T150_PRESET_DEFAULT);

	puts("t150 encode host test ok");
	return 0;
}
