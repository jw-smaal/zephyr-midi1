/**
 * @file main.c
 * @brief MIDI 1.0 Clock Generator Sample Application
 *
 * This sample demonstrates how to use the midi1_clock_cntr driver
 * to generate a 24 PPQN (Pulses Per Quarter Note) MIDI timing clock.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(midi1_clock_gen_sample, LOG_LEVEL_INF);

/* 120.00 BPM scaled by 100 */
#define TARGET_BPM 12000

int main(void)
{
	const struct device *clk_dev = DEVICE_DT_GET_ANY(midi1_clock_cntr);
	char bpm_str[16];

	if (clk_dev == NULL) {
		LOG_ERR("No midi1-clock-cntr device found in devicetree");
		return -ENODEV;
	}

	if (!device_is_ready(clk_dev)) {
		LOG_ERR("MIDI clock device %s is not ready", clk_dev->name);
		return -ENODEV;
	}

	sbpm_to_str(TARGET_BPM, bpm_str, sizeof(bpm_str));
	LOG_INF("Starting MIDI Clock Generator at %s BPM", bpm_str);

	/* Start generating the clock at the specified BPM */
	midi1_clock_cntr_gen(clk_dev, TARGET_BPM);

	while (1) {
		uint16_t current_bpm = midi1_clock_cntr_get_sbpm(clk_dev);
		sbpm_to_str(current_bpm, bpm_str, sizeof(bpm_str));
		LOG_INF("Clock running: %s BPM", bpm_str);
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
