/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file tempo.c
 * @brief Modular Tempo Controller implementation (Digital Encoder).
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include "tempo.h"
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

LOG_MODULE_DECLARE(midi1_arp_phase_app, LOG_LEVEL_INF);

int tempo_init(struct tempo_ctx *ctx, const struct device *clk_dev)
{
	ctx->midi_clock_dev = clk_dev;
	ctx->min_bpm = CONFIG_MIDI1_ARP_TEMPO_MINIMUM;
	ctx->max_bpm = CONFIG_MIDI1_ARP_TEMPO_MAXIMUM;

	/* Initial default BPM */
	ctx->current_bpm = CONFIG_MIDI1_ARP_TARGET_BPM;

	return 0;
}

void tempo_set_bpm(struct tempo_ctx *ctx, int32_t new_bpm)
{
	const struct midi1_clock_cntr_api *clk = ctx->midi_clock_dev->api;

	if (new_bpm < (int32_t)ctx->min_bpm) {
		new_bpm = ctx->min_bpm;
	}
	if (new_bpm > (int32_t)ctx->max_bpm) {
		new_bpm = ctx->max_bpm;
	}

	ctx->current_bpm = new_bpm;

	/* Push update to hardware clock */
	clk->gen(ctx->midi_clock_dev, (uint16_t)ctx->current_bpm);
}
