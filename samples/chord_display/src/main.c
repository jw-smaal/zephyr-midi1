/*
 * SPDX-License-Identifier: Apache-2.0
 */

 /*  
  * This program analyses incoming MIDI notes 
  * and displays the chord on a display. 
  * 
  * @author Jan-Willem Smaal <usenet@gispen.org>
  * @file main.c
  * @date 20260504
  */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include "chord_detector.h"
#include "display.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("Harmonic Chord Detection Sample Started");
	const struct device *midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi));
	if (!device_is_ready(midi_dev)) {
		LOG_ERR("MIDI device not ready");
		return -ENODEV;
	}
	const struct midi1_serial_api *mid = midi_dev->api; 

	chord_detector_init(midi_dev);

	/* Register callbacks from the detector module */
	mid->register_callbacks(midi_dev, 
		(struct midi1_serial_callbacks *)chord_detector_get_callbacks());

	while (1) {
		mid->receiveparser(midi_dev);
		k_yield();
	}

	return 0;
}
