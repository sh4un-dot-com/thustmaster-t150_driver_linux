static void t150_ff_schedule(struct t150 *t150)
{
	if (!t150->gone)
		schedule_work(&t150->ff_work);
}

static void t150_ff_rewrite_rumble(struct ff_effect *effect)
{
	int magnitude;

	if (effect->type != FF_RUMBLE)
		return;

	/* Same weighting as hid-tmff2 / input ff-core, east-facing so the
	 * T150 direction scaling does not collapse the force to zero. */
	magnitude = effect->u.rumble.strong_magnitude / 3
		  + effect->u.rumble.weak_magnitude / 6;

	effect->type = FF_PERIODIC;
	effect->direction = 16384;
	effect->u.periodic.waveform = FF_SINE;
	effect->u.periodic.period = 50;
	effect->u.periodic.magnitude = magnitude;
	effect->u.periodic.offset = 0;
	effect->u.periodic.phase = 0;
	effect->u.periodic.envelope.attack_length = 0;
	effect->u.periodic.envelope.attack_level = 0;
	effect->u.periodic.envelope.fade_length = 0;
	effect->u.periodic.envelope.fade_level = 0;
}

static uint8_t t150_scale_envelope_level(uint16_t level)
{
	if (level > 0x7fff)
		level = 0x7fff;
	return DIV_ROUND_CLOSEST((uint32_t)level * 0xff, 0x7fff);
}

static uint8_t t150_scale_phase(uint16_t phase)
{
	uint32_t p = phase;

	if (p >= 36000)
		p %= 36000;
	return DIV_ROUND_CLOSEST(p * 0xff, 36000);
}

static int8_t t150_scale_coeff(int16_t coeff, uint8_t level)
{
	int32_t v = coeff / 0x147;

	v = v * level / 100;
	if (v > 100)
		v = 100;
	if (v < -100)
		v = -100;
	return (int8_t)v;
}

static uint8_t t150_scale_sat(uint16_t sat, uint16_t divisor, uint8_t level)
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

static inline int t150_init_ffb(struct t150 *t150)
{
	int errno, i;

	t150->ff_effects = kcalloc(T150_FF_MAX_EFFECTS, sizeof(*t150->ff_effects), GFP_KERNEL);
	if (!t150->ff_effects)
		return -ENOMEM;

	for (i = 0; i < t150_ffb_effects_length; i++)
		set_bit(t150_ffb_effects[i], t150->joystick->ffbit);

	errno = input_ff_create(t150->joystick, T150_FF_MAX_EFFECTS);
	if (errno) {
		hid_err(t150->hid_device, "error create ff :(. errno=%i\n", errno);
		kfree(t150->ff_effects);
		t150->ff_effects = NULL;
		return errno;
	}

	t150->joystick->ff->upload = t150_ff_upload;
	t150->joystick->ff->erase = t150_ff_erase;
	t150->joystick->ff->playback = t150_ff_play;
	t150->joystick->ff->set_gain = t150_ff_set_gain;
	t150->joystick->ff->set_autocenter = t150_ff_set_autocenter;

	return 0;
}

static inline void t150_free_ffb(struct t150 *t150)
{
	cancel_work_sync(&t150->ff_work);
	kfree(t150->ff_effects);
	t150->ff_effects = NULL;
}

static void t150_ff_prepare_first(struct ff_first *ff_first, struct ff_effect *effect)
{
	struct ff_envelope *ff_envelope;

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
		ff_envelope = NULL;
		ff_first->f0 = T150_FF_FIRST_CODE_CONDITION;
		break;
	default:
		ff_envelope = NULL;
		break;
	}

	ff_first->pk_id0 = effect->id * 0x1c + 0x1c;
	ff_first->f1 = 0;
	ff_first->f2 = 0x46;
	ff_first->f3 = 0x54;

	if (ff_envelope) {
		ff_first->attack_length = cpu_to_le16(ff_envelope->attack_length);
		ff_first->attack_level = t150_scale_envelope_level(ff_envelope->attack_level);
		ff_first->fade_length = cpu_to_le16(ff_envelope->fade_length);
		ff_first->fade_level = t150_scale_envelope_level(ff_envelope->fade_level);
	}
}

static void t150_ff_prepare_update(struct ff_update *ff_update, struct ff_effect *effect,
				   uint8_t spring_level, uint8_t damper_level)
{
	int32_t level = 0;
	uint8_t cond_level;

	ff_update->pk_id1 = effect->id * 0x1c + 0x0e;
	ff_update->f1 = 0x00;

	switch (effect->type) {
	case FF_PERIODIC:
		ff_update->effect_class = T150_FF_UPDATE_CODE_PERIODIC;
		ff_update->effect.periodic.magnitude = word_high(effect->u.periodic.magnitude);
		ff_update->effect.periodic.offset = word_high(effect->u.periodic.offset);
		ff_update->effect.periodic.phase = t150_scale_phase(effect->u.periodic.phase);
		ff_update->effect.periodic.period = cpu_to_le16(effect->u.periodic.period);
		break;
	case FF_CONSTANT:
		ff_update->effect_class = T150_FF_UPDATE_CODE_CONSTANT;
		level = effect->u.constant.level * fixp_sin16(effect->direction / (0xFFFF / 360)) * +1;
		level >>= 15;
		ff_update->effect.constant.level = (level / 0x01ff);
		break;
	case FF_SPRING:
		cond_level = spring_level;
		ff_update->effect_class = T150_FF_UPDATE_CODE_CONDITION;
		ff_update->effect.condition.right_coeff =
			t150_scale_coeff(effect->u.condition[0].right_coeff, cond_level);
		ff_update->effect.condition.left_coeff =
			t150_scale_coeff(effect->u.condition[0].left_coeff, cond_level);
		ff_update->effect.condition.center = cpu_to_le16(
			effect->u.condition[0].center / (0x7fff / 0x01f4));
		ff_update->effect.condition.deadband = cpu_to_le16(
			effect->u.condition[0].deadband / (0xffff / 0x03e8));
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
		ff_update->effect.condition.center = cpu_to_le16(
			effect->u.condition[0].center / (0x7fff / 0x01f4));
		ff_update->effect.condition.deadband = cpu_to_le16(
			effect->u.condition[0].deadband / (0xffff / 0x03e8));
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

static void t150_ff_prepare_commit(struct ff_commit *ff_commit, struct ff_effect *effect)
{
	ff_commit->f0 = 0x01;
	ff_commit->id = effect->id;
	if (effect->replay.length)
		ff_commit->length = cpu_to_le16(effect->replay.length);
	else
		ff_commit->length = cpu_to_le16(0xffff);
	ff_commit->f1 = 0;
	ff_commit->f2 = 0;
	ff_commit->pk_id1 = effect->id * 0x1c + 0x0e;
	ff_commit->f3 = 0;
	ff_commit->pk_id0 = effect->id * 0x1c + 0x1c;
	ff_commit->f4 = 0;
	ff_commit->delay = min_t(uint16_t, effect->replay.delay, 0xff);
	ff_commit->f5 = 0;

	switch (effect->type) {
	case FF_PERIODIC:
		switch (effect->u.periodic.waveform) {
		case FF_SAW_UP:
			ff_commit->effect_type = cpu_to_le16(T150_FF_COMMIT_CODE_SAW_UP);
			break;
		case FF_SAW_DOWN:
			ff_commit->effect_type = cpu_to_le16(T150_FF_COMMIT_CODE_SAW_DOWN);
			break;
		case FF_SINE:
		default:
			ff_commit->effect_type = cpu_to_le16(T150_FF_COMMIT_CODE_SINE);
			break;
		}
		break;
	case FF_CONSTANT:
		ff_commit->effect_type = cpu_to_le16(T150_FF_COMMIT_CODE_CONSTANT);
		break;
	case FF_SPRING:
		ff_commit->effect_type = cpu_to_le16(T150_FF_COMMIT_CODE_SPRING);
		break;
	case FF_DAMPER:
		ff_commit->effect_type = cpu_to_le16(T150_FF_COMMIT_CODE_DAMPER);
		break;
	default:
		pr_err("t150: unknown effect type: %i\n", effect->type);
		break;
	}
}

static int t150_ff_send_upload(struct t150 *t150, struct t150_ff_effect *slot)
{
	struct ff_first first_old = { 0 }, first_new = { 0 };
	struct ff_update update_old = { 0 }, update_new = { 0 };
	struct ff_commit commit_old = { 0 }, commit_new = { 0 };
	int errno;

	t150_ff_prepare_first(&first_new, &slot->effect);
	t150_ff_prepare_update(&update_new, &slot->effect,
			       t150->spring_level, t150->damper_level);
	t150_ff_prepare_commit(&commit_new, &slot->effect);

	if (slot->has_old) {
		t150_ff_prepare_first(&first_old, &slot->old);
		t150_ff_prepare_update(&update_old, &slot->old,
				       t150->spring_level, t150->damper_level);
		t150_ff_prepare_commit(&commit_old, &slot->old);
	}

	if (T150_FF_BLIND_UPLOAD || !slot->has_old ||
	    memcmp(&first_old, &first_new, sizeof(first_new))) {
		errno = t150_send_int(t150, &first_new, sizeof(first_new));
		if (errno) {
			hid_err(t150->hid_device, "ffb first packet of effect %d failed: %d\n",
				slot->effect.id, errno);
			return errno;
		}
	}

	if (T150_FF_BLIND_UPLOAD || !slot->has_old ||
	    memcmp(&update_old, &update_new, sizeof(update_new))) {
		errno = t150_send_int(t150, &update_new, sizeof(update_new));
		if (errno) {
			hid_err(t150->hid_device, "ffb update packet of effect %d failed: %d\n",
				slot->effect.id, errno);
			return errno;
		}
	}

	if (T150_FF_BLIND_UPLOAD || !slot->has_old ||
	    memcmp(&commit_old, &commit_new, sizeof(commit_new))) {
		errno = t150_send_int(t150, &commit_new, sizeof(commit_new));
		if (errno) {
			hid_err(t150->hid_device, "ffb commit packet of effect %d failed: %d\n",
				slot->effect.id, errno);
			return errno;
		}
	}

	return 0;
}

static int t150_ff_send_play(struct t150 *t150, int effect_id, int times)
{
	struct ff_change_effect_status pkt;
	int errno;

	pkt.f0 = 0x41;
	pkt.id = effect_id;
	pkt.mode = times ? 0x41 : 0x00;
	pkt.times = times ? min_t(int, times, 0xff) : 0x01;

	errno = t150_send_int(t150, &pkt, sizeof(pkt));
	if (errno)
		hid_err(t150->hid_device, "unable to play effect %d, errno %d\n",
			effect_id, errno);
	return errno;
}

static bool t150_ff_work_pending(struct t150 *t150)
{
	int id;

	if (t150->gain_pending || t150->autocenter_pending)
		return true;

	for (id = 0; id < T150_FF_MAX_EFFECTS; id++) {
		if (t150->ff_effects[id].upload_pending ||
		    t150->ff_effects[id].play_pending)
			return true;
	}
	return false;
}

static void t150_ff_work(struct work_struct *work)
{
	struct t150 *t150 = container_of(work, struct t150, ff_work);
	unsigned long flags;
	int id;
	bool gain_pending, ac_pending;
	uint16_t gain, ac;
	struct t150_ff_effect local;
	bool do_upload, do_play;

	if (t150->gone || !t150->ff_effects)
		return;

	spin_lock_irqsave(&t150->ff_lock, flags);
	gain_pending = t150->gain_pending;
	gain = t150->pending_gain;
	t150->gain_pending = false;
	ac_pending = t150->autocenter_pending;
	ac = t150->pending_autocenter;
	t150->autocenter_pending = false;
	spin_unlock_irqrestore(&t150->ff_lock, flags);

	if (gain_pending)
		t150_set_gain(t150, gain);

	if (ac_pending) {
		if (!ac) {
			t150_set_enable_autocenter(t150, false);
		} else {
			t150_set_enable_autocenter(t150, true);
			t150_set_autocenter(t150, ac);
		}
	}

	for (id = 0; id < T150_FF_MAX_EFFECTS; id++) {
		if (t150->gone)
			return;

		spin_lock_irqsave(&t150->ff_lock, flags);
		do_upload = t150->ff_effects[id].upload_pending;
		do_play = t150->ff_effects[id].play_pending;
		if (do_upload || do_play)
			local = t150->ff_effects[id];
		t150->ff_effects[id].upload_pending = false;
		t150->ff_effects[id].play_pending = false;
		spin_unlock_irqrestore(&t150->ff_lock, flags);

		if (do_upload)
			t150_ff_send_upload(t150, &local);
		if (do_play)
			t150_ff_send_play(t150, id, local.play_value);
	}

	spin_lock_irqsave(&t150->ff_lock, flags);
	do_upload = t150_ff_work_pending(t150);
	spin_unlock_irqrestore(&t150->ff_lock, flags);

	if (do_upload)
		t150_ff_schedule(t150);
}

static void t150_ff_resubmit_condition_effects(struct t150 *t150, uint16_t type)
{
	unsigned long flags;
	int id;

	if (!t150->ff_effects)
		return;

	spin_lock_irqsave(&t150->ff_lock, flags);
	for (id = 0; id < T150_FF_MAX_EFFECTS; id++) {
		if (t150->ff_effects[id].effect.type == type) {
			t150->ff_effects[id].has_old = false;
			t150->ff_effects[id].upload_pending = true;
		}
	}
	spin_unlock_irqrestore(&t150->ff_lock, flags);
	t150_ff_schedule(t150);
}

static int t150_ff_upload(struct input_dev *dev, struct ff_effect *effect, struct ff_effect *old)
{
	struct t150 *t150 = input_get_drvdata(dev);
	unsigned long flags;
	struct ff_effect new_e = *effect;
	struct ff_effect old_e;

	if (!t150->ff_effects || effect->id < 0 || effect->id >= T150_FF_MAX_EFFECTS)
		return -EINVAL;

	if (new_e.type == FF_PERIODIC && new_e.u.periodic.period == 0)
		return -EINVAL;

	t150_ff_rewrite_rumble(&new_e);
	if (old) {
		old_e = *old;
		t150_ff_rewrite_rumble(&old_e);
		if (!T150_FF_BLIND_COMPUTE_EFFECT &&
		    memcmp(&new_e, &old_e, sizeof(new_e)) == 0)
			return 0;
	}

	spin_lock_irqsave(&t150->ff_lock, flags);
	if (old) {
		t150->ff_effects[effect->id].old = old_e;
		t150->ff_effects[effect->id].has_old = true;
	} else {
		t150->ff_effects[effect->id].has_old = false;
	}
	t150->ff_effects[effect->id].effect = new_e;
	t150->ff_effects[effect->id].upload_pending = true;
	spin_unlock_irqrestore(&t150->ff_lock, flags);

	t150_ff_schedule(t150);
	return 0;
}

static int t150_ff_erase(struct input_dev *dev, int effect_id)
{
	/* Stopping the effect is already requested through t150_ff_play(times=0).
	 * The wheel has no explicit destroy packet; free the id. */
	return 0;
}

static int t150_ff_play(struct input_dev *dev, int effect_id, int value)
{
	struct t150 *t150 = input_get_drvdata(dev);
	unsigned long flags;

	if (!t150->ff_effects || effect_id < 0 || effect_id >= T150_FF_MAX_EFFECTS)
		return -EINVAL;

	spin_lock_irqsave(&t150->ff_lock, flags);
	t150->ff_effects[effect_id].play_value = value;
	t150->ff_effects[effect_id].play_pending = true;
	spin_unlock_irqrestore(&t150->ff_lock, flags);

	t150_ff_schedule(t150);
	return 0;
}

static void t150_ff_set_gain(struct input_dev *dev, uint16_t gain)
{
	struct t150 *t150 = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&t150->ff_lock, flags);
	t150->pending_gain = gain;
	t150->gain_pending = true;
	spin_unlock_irqrestore(&t150->ff_lock, flags);

	spin_lock_irqsave(&t150->settings.access_lock, flags);
	t150->settings.gain = gain;
	t150->settings.cache_valid = true;
	t150->settings.cache_updated_jiffies = jiffies;
	spin_unlock_irqrestore(&t150->settings.access_lock, flags);

	t150_ff_schedule(t150);
}

static void t150_ff_set_autocenter(struct input_dev *dev, uint16_t value)
{
	struct t150 *t150 = input_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&t150->ff_lock, flags);
	t150->pending_autocenter = value;
	t150->autocenter_pending = true;
	spin_unlock_irqrestore(&t150->ff_lock, flags);

	t150_ff_schedule(t150);
}
