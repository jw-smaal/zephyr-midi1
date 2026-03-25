/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief MIDI 1.0 Hybrid Test Sample Application
 *
 * This sample combines the MIDI 1.0 Clock Generator and the MIDI 1.0
 * Serial Driver to demonstrate the "Safe Hybrid" TX architecture.
 *
 * It generates a 120 BPM clock using the hardware counter driver while
 * simultaneously sending various MIDI messages (Notes and CC) from a
 * background thread.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(midi1_hybrid_test, LOG_LEVEL_INF);

/* 120.00 BPM scaled by 100 */
#define TARGET_BPM 12000

/* Note table for a simple melody */
static const uint8_t melody[] = {60, 62, 64, 65, 67, 69, 71, 72};

/* Minus -1 because MIDI channel 1 = 0 */
#define MY_MIDI1_CHAN (CONFIG_MIDI1_SERIAL_CHANNEL - 1) 


int main(void)
{
	const struct device *midi_serial = DEVICE_DT_GET(DT_NODELABEL(midi0));
	const struct device *midi_clock = DEVICE_DT_GET_ANY(midi1_clock_cntr);
	char bpm_str[16];

	if (!device_is_ready(midi_serial)) {
		LOG_ERR("Serial MIDI device midi0 not ready");
		return -ENODEV;
	}

	if (midi_clock == NULL || !device_is_ready(midi_clock)) {
		LOG_ERR("MIDI clock counter device not ready");
		return -ENODEV;
	}

	sbpm_to_str(TARGET_BPM, bpm_str, sizeof(bpm_str));
	LOG_INF("Starting Hybrid MIDI Test: Clock at %s BPM", bpm_str);

	/* 
	 * 1. Start the hardware-timed MIDI Clock.
	 * This uses the 'midi1_serial_timingclock' inside the driver's
	 * internal Timer ISR, which bypasses the software buffer.
	 */
	midi1_clock_cntr_gen(midi_clock, TARGET_BPM);

	/* 
	 * 2. Start looping through MIDI messages.
	 * These are buffered and sent via interrupts, ensuring the CPU
	 * is free to handle the timing-critical clock pulses.
	 */
	LOG_INF("Sending notes and CC messages in a loop...");

	while (1) {
		/* CC sweep (simulating continuous controller data) */
		for (uint8_t i = 0; i < 127; i++) {
			midi1_serial_control_change(midi_serial, 
						    MY_MIDI1_CHAN, 
						    CTL_MSB_MODWHEEL, 
						    i);
			k_sleep(K_MSEC(10));
		}

		/* Melody sweep */
		for (int i = 0; i < sizeof(melody); i++) {
			midi1_serial_note_on(midi_serial, MY_MIDI1_CHAN, melody[i], 100);
			k_sleep(K_MSEC(250));
			midi1_serial_note_off(midi_serial, MY_MIDI1_CHAN, melody[i], 0);
			k_sleep(K_MSEC(50));
		}

		LOG_INF("Melody loop complete. Repeating...");
	}

	return 0;
}
