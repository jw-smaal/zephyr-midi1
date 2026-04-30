/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260429
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/midi1/harmonic.h>

LOG_MODULE_REGISTER(midi_reactive, LOG_LEVEL_INF);

#define MOD_WHEEL_CC 1
#define NOTES_PER_OCTAVE 12u

static const struct device *midi_dev;
static uint8_t current_mod_wheel = 0;
static harmonic_mask_t current_scale = HARMONIC_MASK_MAJOR;

/* Callback when a Note On is received */
static void midi_note_on_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	LOG_INF("Note On: %d (vel %d), Wheel: %d", note, velocity, current_mod_wheel);

	/* Generate notes in the current scale starting from the root note */
	for (uint8_t i = 0; i < NOTES_PER_OCTAVE; i++) {
		if (current_scale & (1u << i)) {
			uint8_t output_note = note + i;
			if (output_note <= 127) {
				/* Send note back out */
				midi1_serial_note_on(midi_dev, channel, output_note, velocity);
			}
		}
	}
}

/* Callback when a Note Off is received */
static void midi_note_off_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	/* Kill all notes in the scale for this root */
	for (uint8_t i = 0; i < NOTES_PER_OCTAVE; i++) {
		if (current_scale & (1u << i)) {
			uint8_t output_note = note + i;
			if (output_note <= 127) {
				midi1_serial_note_off(midi_dev, channel, output_note, velocity);
			}
		}
	}
}

/* Callback when a Control Change is received */
static void midi_cc_cb(uint8_t channel, uint8_t controller, uint8_t value)
{
	if (controller == MOD_WHEEL_CC) {
		current_mod_wheel = value;
		
		/* Switch scale based on Mod Wheel value */
		if (value == 0) {
			current_scale = HARMONIC_MASK_MAJOR;
			LOG_INF("Scale switched to MAJOR");
		} else if (value >= 1 && value <= 32) {
			current_scale = HARMONIC_MASK_NATURAL_MINOR;
			LOG_INF("Scale switched to NATURAL MINOR");
		} else if (value >= 33 && value <= 64) {
			current_scale = HARMONIC_MASK_DORIAN;
			LOG_INF("Scale switched to DORIAN");
		} else if (value >= 65 && value <= 96) {
			current_scale = HARMONIC_MASK_BLUES;
			LOG_INF("Scale switched to BLUES");
		} else {
			current_scale = HARMONIC_MASK_CHROMATIC;
			LOG_INF("Scale switched to CHROMATIC");
		}
	}
}

/* MIDI callback structure */
static const struct midi1_serial_callbacks midi_cbs = {
	.note_on = midi_note_on_cb,
	.note_off = midi_note_off_cb,
	.control_change = midi_cc_cb,
};

int main(void)
{
	LOG_INF("MIDI Reactive Harmonic Sample Started");

	midi_dev = DEVICE_DT_GET_ANY(midi1_serial);
	if (!device_is_ready(midi_dev)) {
		LOG_ERR("MIDI device not ready");
		return -ENODEV;
	}

	/* Register callbacks */
	midi1_serial_register_callbacks(midi_dev, (struct midi1_serial_callbacks *)&midi_cbs);

	LOG_INF("Listening for MIDI events...");

	while (1) {
		k_sleep(K_FOREVER);
	}

	return 0;
}
