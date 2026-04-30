/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260429
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/midi1/harmonic.h>

LOG_MODULE_REGISTER(harmonic_chord_gen, LOG_LEVEL_INF);

#define NOTES_PER_OCTAVE 12u

/**
 * @brief Simple chord generation logic.
 * 
 * This sample takes a root note and generates MIDI note numbers for a chord
 * based on a harmonic mask.
 */
void generate_chord(uint8_t root_note, harmonic_mask_t chord_mask)
{
	uint8_t root_val = root_note % NOTES_PER_OCTAVE;
	uint8_t octave = root_note / NOTES_PER_OCTAVE;

	LOG_INF("Generating chord for root %s (%d):", 
	        harmonic_get_note_name(root_val, true), root_note);

	for (uint8_t i = 0; i < NOTES_PER_OCTAVE; i++) {
		if (chord_mask & (1u << i)) {
			uint8_t note_val = (root_val + i);
			uint8_t final_note = (octave * NOTES_PER_OCTAVE) + note_val;
			
			if (final_note <= 127) {
				LOG_INF("  Note: %s (%d)", 
				        harmonic_get_note_name(final_note % NOTES_PER_OCTAVE, true), 
				        final_note);
			}
		}
	}
}

int main(void)
{
	LOG_INF("Harmonic Chord Generator Sample Started");

	/* Example 1: C Major Triad (Root C4 = 60) */
	generate_chord(60, HARMONIC_MASK_MAJOR_TRIAD);

	k_sleep(K_MSEC(500));

	/* Example 2: A Minor 7th (Root A3 = 57) */
	generate_chord(57, HARMONIC_MASK_MINOR_7TH);

	k_sleep(K_MSEC(500));

	/* Example 3: G Dominant 7th (Root G4 = 67) */
	generate_chord(67, HARMONIC_MASK_DOMINANT_7TH);

	/* Example 4: D Dorian (C Major rotated to 2nd degree, Root D3 = 50) */
	harmonic_mask_t dorian = harmonic_rotate_mask(HARMONIC_MASK_MAJOR, 1);
	generate_chord(50, dorian);

	LOG_INF("Sample execution complete.");
	return 0;
}
