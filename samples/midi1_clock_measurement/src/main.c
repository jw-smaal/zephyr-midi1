/**
 * @file main.c
 * @brief MIDI 1.0 Clock Measurement Sample Application
 *
 * This sample demonstrates how to use the midi1_clock_meas_cntr driver
 * to measure the BPM of an incoming MIDI timing clock signal.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_clock_meas_cntr.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(midi1_clock_meas_sample, LOG_LEVEL_INF);

int main(void)
{
	const struct device *meas_dev = DEVICE_DT_GET_ANY(midi1_clock_meas_cntr);
	char bpm_str[16];

	if (meas_dev == NULL) {
		LOG_ERR("No midi1-clock-meas-cntr device found in devicetree");
		return -ENODEV;
	}

	if (!device_is_ready(meas_dev)) {
		LOG_ERR("MIDI measurement device %s is not ready", meas_dev->name);
		return -ENODEV;
	}

	LOG_INF("Starting MIDI Clock Measurement...");

	/* Start measuring the clock */
	midi1_clock_meas_cntr_start(meas_dev);

	while (1) {
		uint16_t current_bpm = midi1_clock_meas_cntr_get_sbpm(meas_dev);
		sbpm_to_str(current_bpm, bpm_str, sizeof(bpm_str));
		LOG_INF("Incoming Clock: %s BPM", bpm_str);
		k_sleep(K_SECONDS(2));
	}

	return 0;
}
