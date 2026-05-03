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

#define MOD_WHEEL_CC     1
#define NOTES_PER_OCTAVE 12u

const struct device *midi_dev;
static uint8_t current_mod_wheel = 0;
static harm_mask_t current_mask = HARM_MASK_MAJOR;

/* Callback when a Note On is received */
static void midi_note_on_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	LOG_INF("Note On: %d (vel %d), Wheel: %d", note, velocity, current_mod_wheel);

	/* Generate notes in the current mask starting from the root note */
	for (uint8_t i = 0; i < NOTES_PER_OCTAVE; i++) {
		if (current_mask & (1u << i)) {
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
	/* Kill all notes in the mask for this root */
	for (uint8_t i = 0; i < NOTES_PER_OCTAVE; i++) {
		if (current_mask & (1u << i)) {
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

		/* Switch harmonic mask based on Mod Wheel value
		 * Partitioned into ~8 units per mode (128 / 15 modes)
		 */
		if (value <= 8) {
			current_mask = HARM_MASK_MAJOR;
			LOG_INF("Mode: MAJOR SCALE");
		} else if (value <= 16) {
			current_mask = HARM_MASK_NATURAL_MINOR;
			LOG_INF("Mode: NATURAL MINOR SCALE");
		} else if (value <= 24) {
			current_mask = HARM_MASK_HARMONIC_MINOR;
			LOG_INF("Mode: HARMONIC MINOR SCALE");
		} else if (value <= 32) {
			current_mask = HARM_MASK_DORIAN;
			LOG_INF("Mode: DORIAN SCALE");
		} else if (value <= 40) {
			current_mask = HARM_MASK_PHRYGIAN;
			LOG_INF("Mode: PHRYGIAN SCALE");
		} else if (value <= 48) {
			current_mask = HARM_MASK_LYDIAN;
			LOG_INF("Mode: LYDIAN SCALE");
		} else if (value <= 56) {
			current_mask = HARM_MASK_MIXOLYDIAN;
			LOG_INF("Mode: MIXOLYDIAN SCALE");
		} else if (value <= 64) {
			current_mask = HARM_MASK_BLUES;
			LOG_INF("Mode: BLUES SCALE");
		} else if (value <= 72) {
			current_mask = HARM_MASK_MAJOR_7TH;
			LOG_INF("Mode: MAJOR 7TH CHORD");
		} else if (value <= 80) {
			current_mask = HARM_MASK_MINOR_7TH;
			LOG_INF("Mode: MINOR 7TH CHORD");
		} else if (value <= 88) {
			current_mask = HARM_MASK_DOMINANT_7TH;
			LOG_INF("Mode: DOMINANT 7TH CHORD");
		} else if (value <= 96) {
			current_mask = HARM_MASK_MINOR_MAJOR_7TH;
			LOG_INF("Mode: MINOR-MAJOR 7TH CHORD");
		} else if (value <= 104) {
			current_mask = HARM_MASK_DIMINISHED_7TH;
			LOG_INF("Mode: DIMINISHED 7TH CHORD");
		} else if (value <= 112) {
			current_mask = HARM_MASK_SUS4;
			LOG_INF("Mode: SUS4 CHORD");
		} else if (value <= 120) {
			current_mask = HARM_MASK_WHOLE_TONE;
			LOG_INF("Mode: WHOLE TONE SCALE");
		} else {
			current_mask = HARM_MASK_CHROMATIC;
			LOG_INF("Mode: CHROMATIC");
		}
	}
}
/* MIDI callback structure */
const struct midi1_serial_callbacks midi_cbs = {
	.note_on = midi_note_on_cb,
	.note_off = midi_note_off_cb,
	.control_change = midi_cc_cb,
};

int main(void)
{
	LOG_INF("MIDI Reactive Harmonic Sample Started");

	midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi1));
	if (!device_is_ready(midi_dev)) {
		LOG_ERR("MIDI device not ready");
		return -ENODEV;
	}

	/* Register callbacks */
	midi1_serial_register_callbacks(midi_dev, (struct midi1_serial_callbacks *)&midi_cbs);

	while (1) {
		midi1_serial_receiveparser(midi_dev);
		k_yield();
	}

	return 0;
}
