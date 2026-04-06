/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TEMPO_H_
#define TEMPO_H_

#include <zephyr/device.h>
#include <zephyr/kernel.h>

/**
 * @file tempo.h
 * @brief Modular Tempo Controller for Encoder-based BPM management.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

struct tempo_ctx {
	const struct device *midi_clock_dev;

	int32_t current_bpm; /* BPM * 100 */
	uint32_t min_bpm;
	uint32_t max_bpm;
};

/**
 * @brief Initialize the tempo controller.
 *
 * @param ctx The tempo context to initialize.
 * @param clk_dev The MIDI clock counter device to update.
 * @return 0 on success, negative errno on failure.
 */
int tempo_init(struct tempo_ctx *ctx, const struct device *clk_dev);

/**
 * @brief Directly update the BPM value and the hardware MIDI clock.
 *
 * @param ctx The tempo context.
 * @param new_bpm The new BPM (times 100).
 */
void tempo_set_bpm(struct tempo_ctx *ctx, int32_t new_bpm);

#endif /* TEMPO_H_ */
