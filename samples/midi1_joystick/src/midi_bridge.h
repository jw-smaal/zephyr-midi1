/*
 * midi_bridge.h - MIDI translation logic for joystick inputs
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIDI_BRIDGE_H
#define MIDI_BRIDGE_H

#include <zephyr/kernel.h>
#include <stdint.h>

/**
 * @brief Structure representing the state of the joystick
 */
struct joystick_state {
	uint16_t x;
	uint16_t y;
	uint16_t buttons;
	uint8_t twist;
	uint8_t fader;
};

/**
 * @brief Initialize the MIDI bridge subsystem
 * 
 * @return 0 on success, negative error code otherwise
 */
int midi_bridge_init(void);

/**
 * @brief Process a new joystick state and send corresponding MIDI messages
 * 
 * @param state Pointer to the current joystick state
 */
void midi_bridge_process(const struct joystick_state *state);

#endif /* MIDI_BRIDGE_H */
