/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arp.h"

void arp_init(struct arp_ctx *ctx, int interval, int drift_cycle)
{
	ctx->num_notes = 0;
	ctx->physical_keys_count = 0;
	ctx->arp_running = false;
	ctx->latch_enabled = false;

	ctx->base.index = 0;
	ctx->base.clock_counter = 0;
	ctx->base.playing_pitch = 0;

	ctx->high.index = 0;
	ctx->high.clock_counter = 0;
	ctx->high.playing_pitch = 0;
	ctx->high.drift_trigger_count = 0;
	ctx->high.lag_pending = false;

	ctx->timing_interval = interval;
	ctx->drift_cycle = drift_cycle;
	ctx->mode = ARP_MODE_SYNC;

	ctx->process_len = 1;
	ctx->process_offset = 0;
	ctx->process_building = true;

	k_mutex_init(&ctx->lock);
}

void arp_note_on(struct arp_ctx *ctx, uint8_t pitch, uint8_t velocity)
{
	k_mutex_lock(&ctx->lock, K_FOREVER);

	/* If this is the start of a new chord, clear old latched notes */
	if (ctx->latch_enabled && ctx->physical_keys_count == 0) {
		ctx->num_notes = 0;
		/* Reset phasing state on new chord */
		ctx->base.clock_counter = 0;
		ctx->high.clock_counter = 0;
		ctx->high.drift_trigger_count = 0;
		ctx->high.lag_pending = false;

		/* Reset process state */
		ctx->process_len = 1;
		ctx->process_offset = 0;
		ctx->process_building = true;
		ctx->base.index = -1; /* Will increment to 0 on first tick */
		ctx->high.index = 1;  /* Will decrement to 0 on first tick */
	}

	ctx->physical_keys_count++;

	if (ctx->num_notes < ARP_MAX_NOTES) {
		ctx->notes[ctx->num_notes].pitch = pitch;
		ctx->notes[ctx->num_notes].velocity = velocity;
		ctx->num_notes++;
	}

	k_mutex_unlock(&ctx->lock);
}

void arp_note_off(struct arp_ctx *ctx, uint8_t pitch)
{
	k_mutex_lock(&ctx->lock, K_FOREVER);

	if (ctx->physical_keys_count > 0) {
		ctx->physical_keys_count--;
	}

	for (int i = 0; i < ctx->num_notes; i++) {
		if (ctx->notes[i].pitch == pitch) {
			if (!ctx->latch_enabled) {
				/* Remove note and shift buffer */
				for (int j = i; j < ctx->num_notes - 1; j++) {
					ctx->notes[j] = ctx->notes[j + 1];
				}
				ctx->num_notes--;
			}
			break;
		}
	}

	k_mutex_unlock(&ctx->lock);
}

void arp_set_latch(struct arp_ctx *ctx, bool enabled)
{
	k_mutex_lock(&ctx->lock, K_FOREVER);
	ctx->latch_enabled = enabled;
	if (!ctx->latch_enabled && ctx->physical_keys_count == 0) {
		ctx->num_notes = 0;
	}
	k_mutex_unlock(&ctx->lock);
}

void arp_set_mode(struct arp_ctx *ctx, enum arp_mode mode)
{
	k_mutex_lock(&ctx->lock, K_FOREVER);
	enum arp_mode old_mode = ctx->mode;
	ctx->mode = mode;

	/* If disabling phasing, snap back */
	bool old_phasing = (old_mode == ARP_MODE_PHASE || old_mode == ARP_MODE_PHASE_PROCESS);
	bool new_phasing = (mode == ARP_MODE_PHASE || mode == ARP_MODE_PHASE_PROCESS);

	if (old_phasing && !new_phasing) {
		ctx->high.drift_trigger_count = 0;
		ctx->high.lag_pending = false;
		ctx->high.clock_counter = ctx->base.clock_counter;
	}

	/* Reset process state if entering/leaving process mode */
	bool old_process = (old_mode == ARP_MODE_PROCESS || old_mode == ARP_MODE_PHASE_PROCESS);
	bool new_process = (mode == ARP_MODE_PROCESS || mode == ARP_MODE_PHASE_PROCESS);

	if (old_process != new_process) {
		ctx->process_len = 1;
		ctx->process_offset = 0;
		ctx->process_building = true;
		if (ctx->num_notes > 0) {
			ctx->base.index = -1;
			ctx->high.index = 1;
		}
	}

	k_mutex_unlock(&ctx->lock);
}

/**
 * @brief Logic to advance the process window.
 */
static void advance_process_window(struct arp_ctx *ctx)
{
	if (ctx->process_building) {
		if (ctx->process_len < ctx->num_notes) {
			ctx->process_len++;
		} else {
			/* Start tearing down */
			ctx->process_building = false;
			ctx->process_offset++;
			ctx->process_len--;
		}
	} else {
		if (ctx->process_len > 1) {
			ctx->process_offset++;
			ctx->process_len--;
		} else {
			/* Reset to build */
			ctx->process_building = true;
			ctx->process_offset = 0;
			ctx->process_len = 1;
		}
	}
}

struct arp_tick_result arp_tick(struct arp_ctx *ctx)
{
	struct arp_tick_result result = {0};
	bool is_process = (ctx->mode == ARP_MODE_PROCESS || ctx->mode == ARP_MODE_PHASE_PROCESS);
	bool is_phasing = (ctx->mode == ARP_MODE_PHASE || ctx->mode == ARP_MODE_PHASE_PROCESS);

	k_mutex_lock(&ctx->lock, K_FOREVER);

	if (ctx->num_notes == 0) {
		ctx->arp_running = false;
		k_mutex_unlock(&ctx->lock);
		return result;
	}

	ctx->base.clock_counter++;
	ctx->high.clock_counter++;

	/* 1. Process Base Layer */
	if (ctx->base.clock_counter >= ctx->timing_interval) {
		ctx->base.clock_counter = 0;

		/* Index arithmetic based on mode */
		if (is_process) {
			ctx->base.index++;
			if (ctx->base.index >= (ctx->process_offset + ctx->process_len)) {
				/* Full traversal of window complete */
				advance_process_window(ctx);
				ctx->base.index = ctx->process_offset;
			}
			/* Bound safety */
			if (ctx->base.index < ctx->process_offset) {
				ctx->base.index = ctx->process_offset;
			}
		} else {
			if (++ctx->base.index >= ctx->num_notes) {
				ctx->base.index = 0;
			}
		}

		ctx->base.playing_pitch = ctx->notes[ctx->base.index].pitch;
		result.base_pitch = ctx->base.playing_pitch;
		result.velocity = ctx->notes[ctx->base.index].velocity;
		result.base_triggered = true;
		ctx->arp_running = true;
	}

	/* 2. Process High Layer */
	int high_interval = ctx->timing_interval;
	if (is_phasing && ctx->high.lag_pending) {
		high_interval += 1;
	}

	if (ctx->high.clock_counter >= high_interval) {
		ctx->high.clock_counter = 0;
		ctx->high.lag_pending = false;

		if (is_process) {
			/* High layer mirrored reverse window traversal */
			ctx->high.index--;
			/* Window boundaries for reverse: [N-1-offset-len+1, N-1-offset] */
			int high_start = ctx->num_notes - 1 - ctx->process_offset;
			int high_end = high_start - ctx->process_len + 1;

			if (ctx->high.index < high_end) {
				ctx->high.index = high_start;
			}
			/* Bound safety */
			if (ctx->high.index > high_start) {
				ctx->high.index = high_start;
			}
		} else {
			if (--ctx->high.index < 0) {
				ctx->high.index = ctx->num_notes - 1;
			}
		}

		int pitch_calc = ctx->notes[ctx->high.index].pitch + 12;
		if (pitch_calc > 127) {
			ctx->high.playing_pitch = 127;
		} else {
			ctx->high.playing_pitch = (uint8_t)pitch_calc;
		}
		result.high_pitch = ctx->high.playing_pitch;

		/*
		 * If base wasn't triggered, use high layer's velocity.
		 * If both triggered, base velocity usually takes priority
		 * or we just use the last calculated.
		 */
		if (!result.base_triggered) {
			result.velocity = ctx->notes[ctx->high.index].velocity;
		}

		result.high_triggered = true;

		if (is_phasing) {
			if (++ctx->high.drift_trigger_count >= ctx->drift_cycle) {
				ctx->high.drift_trigger_count = 0;
				ctx->high.lag_pending = true;
			}
		}
	}

	k_mutex_unlock(&ctx->lock);
	return result;
}

void arp_get_current_pitches(struct arp_ctx *ctx, uint8_t *out_base, uint8_t *out_high)
{
	k_mutex_lock(&ctx->lock, K_FOREVER);
	*out_base = ctx->base.playing_pitch;
	*out_high = ctx->high.playing_pitch;
	k_mutex_unlock(&ctx->lock);
}

bool arp_is_running(struct arp_ctx *ctx)
{
	return ctx->arp_running;
}

int arp_get_num_notes(struct arp_ctx *ctx)
{
	return ctx->num_notes;
}
