/*
 * SPDX-License-Identifier: Apache-2.0
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @file main.c
 * @date 20260504
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include "chord_detector.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	const struct device *midi_dev;

	LOG_INF("Harmonic Chord Detection Sample Started");

	midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi1));
	if (!device_is_ready(midi_dev)) {
		LOG_ERR("MIDI device not ready");
		return -ENODEV;
	}

	chord_detector_init(midi_dev);

	/* Register callbacks from the detector module */
	midi1_serial_register_callbacks(midi_dev, 
		(struct midi1_serial_callbacks *)chord_detector_get_callbacks());

	while (1) {
		midi1_serial_receiveparser(midi_dev);
		k_yield();
	}

	return 0;
}
