/*
 * midi_bridge.c - MIDI translation logic for joystick inputs
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/logging/log.h>
#include "midi_bridge.h"

LOG_MODULE_DECLARE(main, LOG_LEVEL_INF);

static const struct device *midi_dev;
static uint16_t last_buttons;
static uint8_t last_fader;

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

	return ret;
}

static void process_axes(const struct joystick_state *state)
{
	if (device_is_ready(midi_dev)) {
		/* Map 14-bit X to 14-bit Pitchwheel */
		midi1_serial_pitchwheel(midi_dev, 0, state->x);

		/* Map 14-bit Y to 7-bit Mod Wheel (CC 1) */
		midi1_serial_control_change(midi_dev, 0, 1, state->y >> 7);

		printk("STICK X:%-5u Y:%-5u | MIDI -> PW:%-5u MOD:%-3u\n",
		       state->x, state->y, state->x, state->y >> 7);
	}
}

static void process_fader(const struct joystick_state *state)
{
	if (state->fader != last_fader) {
		if (device_is_ready(midi_dev)) {
			/* Map 8-bit fader to 7-bit Volume (CC 7) */
			midi1_serial_control_change(midi_dev, 0, 7, state->fader >> 1);
			printk("STICK FADER:%-3u    | MIDI -> VOL:%-3u\n",
			       state->fader, state->fader >> 1);
		}
		last_fader = state->fader;
	}
}

static void process_buttons(const struct joystick_state *state)
{
	/* Check Fire Button (Bit 0) */
	if ((state->buttons & BIT(0)) != (last_buttons & BIT(0))) {
		if (device_is_ready(midi_dev)) {
			if (state->buttons & BIT(0)) {
				midi1_serial_note_on(midi_dev, 0, 60, 100);
				printk("STICK FIRE:DOWN    | MIDI -> NOTE 60 ON\n");
			} else {
				midi1_serial_note_off(midi_dev, 0, 60, 100);
				printk("STICK FIRE:UP      | MIDI -> NOTE 60 OFF\n");
			}
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
