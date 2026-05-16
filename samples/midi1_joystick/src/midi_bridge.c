/*
 * midi_bridge.c - MIDI translation logic for joystick inputs
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/logging/log.h>
#include "midi_bridge.h"

LOG_MODULE_DECLARE(main, LOG_LEVEL_INF);

static const struct device *midi_dev;
static const struct midi1_serial_api *mid;

static uint16_t last_buttons;
static uint8_t last_fader;
static uint16_t last_pw;
static uint8_t last_mod;
static uint8_t last_cutoff;

int midi_bridge_init(void)
{
	int ret = 0;

	midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi0));
	if (device_is_ready(midi_dev)) {
		printk("MIDI device %s is ready\n", midi_dev->name);
		ret = 0;
	} else {
		LOG_ERR("MIDI device not ready");
		ret = -ENODEV;
	}
	/* Assign api pointer */
	mid = midi_dev->api;

	return ret;
}

static void process_axes(const struct joystick_state *state)
{
	uint16_t current_pw = state->x;
	uint8_t current_mod = 0;
	uint8_t current_cutoff = state->twist >> 1;

	/* 
	 * MOD WHEEL: Only forward Y axis push (Stick Forward)
	 * T.16000M "Forward" values are < 8192 (Neutral is 8192).
	 */
	if (state->y < 8192) {
		/* Scale [8191 center down to 0 full push] to [1 up to 127] */
		uint16_t delta = 8192 - state->y;
		current_mod = (uint8_t)(delta >> 6);
		if (current_mod > 127) {
			current_mod = 127;
		}
	} else {
		/* Neutral or pulled back */
		current_mod = 0;
	}

	if (device_is_ready(midi_dev)) {
		/* Send Pitchwheel if changed */
		if (current_pw != last_pw) {
			mid->pitchwheel(midi_dev, 
					CH1, 
					current_pw);
			printk("STICK X:%-5u | MIDI -> PitchWheel:%-5u\n", state->x, current_pw);
			last_pw = current_pw;
		}

		/* Send Modulation Wheel if changed */
		if (current_mod != last_mod) {
			mid->control_change(midi_dev, 
					    CH1, 
					    CTL_MSB_MODWHEEL, 
					    current_mod);
			printk("STICK Y:%-5u | MIDI -> ModWheel:%-3u\n", state->y, current_mod);
			last_mod = current_mod;
		}


		/* Filter Cutoff (Twist/Yaw) -> CC 74 */
		if (current_cutoff != last_cutoff) {
			mid->control_change(midi_dev, 
				            CH1, 
					    CTL_SC5_BRIGHTNESS, 
					    current_cutoff);
			printk("STICK T:%-3u   | MIDI -> FilterCutoff:%-3u\n", state->twist, current_cutoff);
			last_cutoff = current_cutoff;
		}
	}
}

static void process_fader(const struct joystick_state *state)
{
	if (state->fader != last_fader) {
		if (device_is_ready(midi_dev)) {
			/* Invert fader: Forward (0) = 127, Back (255) = 0 */
			uint8_t current_vol = 127 - (state->fader >> 1);
			mid->control_change(midi_dev, 
					    CH1, 
					    CTL_MSB_MAIN_VOLUME, 
					    current_vol);
			printk("STICK FADER:%-3u    | MIDI -> VOL:%-3u\n",
			       state->fader, current_vol);
		}
		last_fader = state->fader;
	}
}

static void handle_button_note(uint8_t button_idx, bool pressed)
{
	/* Map 12 buttons to chromatic octave starting at C4 (Note 60) */
	uint8_t note = 60 + button_idx;

	if (device_is_ready(midi_dev)) {
		if (pressed) {
			midi1_serial_note_on(midi_dev, CH1, note, 100);
			printk("BUTTON %u:DOWN    | MIDI -> NOTE %u ON\n", button_idx, note);
		} else {
			midi1_serial_note_off(midi_dev, CH1, note, 100);
			printk("BUTTON %u:UP      | MIDI -> NOTE %u OFF\n", button_idx, note);
		}
	}
}

static void process_buttons(const struct joystick_state *state)
{
	/* Process 12 buttons (Bit 0 to 11) */
	for (uint8_t i = 0; i < 12; i++) {
		uint16_t mask = BIT(i);
		bool current_pressed = (state->buttons & mask) != 0;
		bool last_pressed = (last_buttons & mask) != 0;

		if (current_pressed != last_pressed) {
			handle_button_note(i, current_pressed);
		}
	}
	last_buttons = state->buttons;
}

void midi_bridge_process(const struct joystick_state *state)
{
	process_axes(state);
	process_fader(state);
	process_buttons(state);
}

/* EOF */ 
