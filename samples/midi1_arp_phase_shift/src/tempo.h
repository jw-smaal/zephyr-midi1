/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEMPO_H_
#define TEMPO_H_

#include <zephyr/drivers/adc.h>
#include <zephyr/device.h>

/**
 * @file tempo.h
 * @brief Modular Tempo Controller for ADC-based BPM management.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

struct tempo_ctx {
	const struct device *midi_clock_dev;
	struct adc_dt_spec adc_spec;
	int16_t adc_buf;
	struct adc_sequence sequence;

	int32_t last_bpm;
	uint32_t min_bpm;
	uint32_t max_bpm;
	uint32_t deadband;
};

/**
 * @brief Initialize the tempo controller.
 *
 * @param ctx The tempo context to initialize.
 * @param clk_dev The MIDI clock counter device to update.
 * @param adc_idx The index of the ADC channel in the zephyr,user node.
 * @return 0 on success, negative errno on failure.
 */
int tempo_init(struct tempo_ctx *ctx, const struct device *clk_dev,
	       uint8_t adc_idx);

/**
 * @brief Sample the ADC and update the MIDI clock tempo if needed.
 *
 * @param ctx The tempo context.
 */
void tempo_update(struct tempo_ctx *ctx);

#endif /* TEMPO_H_ */
