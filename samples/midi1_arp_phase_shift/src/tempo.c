/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file tempo.c
 * @brief Modular Tempo Controller implementation.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include "tempo.h"
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/logging/log.h>
#include <stdlib.h>

LOG_MODULE_DECLARE(midi1_arp_phase_app, LOG_LEVEL_WRN);

int tempo_init(struct tempo_ctx *ctx, const struct device *clk_dev,
	       uint8_t adc_idx)
{
	int err;

	ctx->midi_clock_dev = clk_dev;
	ctx->min_bpm = 500;   /* 5.00 BPM */
	ctx->max_bpm = 36000; /* 360.00 BPM */
	ctx->deadband = 60;  /* 0.60 BPM */
	ctx->last_bpm = 0;

	/* Setup ADC spec from zephyr,user node */
	struct adc_dt_spec spec = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
	ctx->adc_spec = spec;

	if (!device_is_ready(ctx->adc_spec.dev)) {
		LOG_ERR("ADC device not ready");
		return -ENODEV;
	}

	err = adc_channel_setup_dt(&ctx->adc_spec);
	if (err < 0) {
		LOG_ERR("ADC channel setup failed (err %d)", err);
		return err;
	}

	ctx->sequence.buffer = &ctx->adc_buf;
	ctx->sequence.buffer_size = sizeof(ctx->adc_buf);

	err = adc_sequence_init_dt(&ctx->adc_spec, &ctx->sequence);
	if (err < 0) {
		LOG_ERR("ADC sequence init failed (err %d)", err);
		return err;
	}

	return 0;
}

void tempo_update(struct tempo_ctx *ctx)
{
	int err;
	int32_t current_bpm;
	const struct midi1_clock_cntr_api *clk = ctx->midi_clock_dev->api;

	err = adc_read_dt(&ctx->adc_spec, &ctx->sequence);
	if (err < 0) {
		LOG_ERR("ADC read failed (err %d)", err);
		return;
	}

	/* Map 12-bit ADC (0-4095) to min_bpm - max_bpm range */
	current_bpm = (int32_t)ctx->min_bpm +
		      ((int32_t)ctx->adc_buf *
		       (int32_t)(ctx->max_bpm - ctx->min_bpm) / 4095);

	/* Apply deadband to avoid jitter */
	if (abs(current_bpm - ctx->last_bpm) > (int32_t)ctx->deadband) {
		clk->gen(ctx->midi_clock_dev, (uint16_t)current_bpm);
		ctx->last_bpm = current_bpm;
		LOG_INF("BPM Updated: %d.%02d", current_bpm / 100,
			current_bpm % 100);
	}
}
