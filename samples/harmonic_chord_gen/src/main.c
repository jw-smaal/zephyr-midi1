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
void generate_chord(uint8_t root_note, harm_mask_t chord_mask)
{
	uint8_t root_val = root_note % NOTES_PER_OCTAVE;
	uint8_t octave = root_note / NOTES_PER_OCTAVE;

	LOG_INF("Generating %s for root %s (%d):", 
	        harm_recognize_chord(chord_mask),
	        harm_get_note_name(root_val, true), root_note);

	for (uint8_t i = 0; i < NOTES_PER_OCTAVE; i++) {
		if (chord_mask & (1u << i)) {
			uint8_t note_val = (root_val + i);
			uint8_t final_note = (octave * NOTES_PER_OCTAVE) + note_val;
			
			if (final_note <= 127) {
				LOG_INF("  Note: %s (%d)", 
				        harm_get_note_name(final_note % NOTES_PER_OCTAVE, true), 
				        final_note);
			}
		}
	}
}

int main(void)
{
	LOG_INF("Harmonic Chord Generator Sample Started");

	/* Example 1: C Major Triad (Root C4 = 60) */
	generate_chord(60, HARM_MASK_MAJOR_TRIAD);

	k_sleep(K_MSEC(500));

	/* Example 2: A Minor 7th (Root A3 = 57) */
	generate_chord(57, HARM_MASK_MINOR_7TH);

	k_sleep(K_MSEC(500));

	/* Example 3: G Dominant 7th (Root G4 = 67) */
	generate_chord(67, HARM_MASK_DOMINANT_7TH);

	/* Example 4: D Dorian (C Major rotated to 2nd degree, Root D3 = 50) */
	harm_mask_t dorian = harm_rotate_mask(HARM_MASK_MAJOR, 1);
	LOG_INF("Dorian is mode of Major scale. Major scale has %d distinct modes.",
	        harm_get_distinct_mode_count(HARM_MASK_MAJOR));
	generate_chord(50, dorian);

	k_sleep(K_MSEC(500));

	/* Example 5: Whole Tone scale (Symmetric) */
	uint8_t wt_modes = harm_get_distinct_mode_count(HARM_MASK_WHOLE_TONE);
	LOG_INF("Whole Tone scale has %d notes and %d distinct modes",
	        harm_get_mask_note_count(HARM_MASK_WHOLE_TONE), wt_modes);
	generate_chord(60, HARM_MASK_WHOLE_TONE);

	k_sleep(K_MSEC(500));

	/* Example 6: Octatonic scale (Symmetric) */
	uint8_t oct_modes = harm_get_distinct_mode_count(HARM_MASK_OCTATONIC_HW);
	LOG_INF("Octatonic (H-W) scale has %d notes and %d distinct modes",
	        harm_get_mask_note_count(HARM_MASK_OCTATONIC_HW), oct_modes);
	generate_chord(60, HARM_MASK_OCTATONIC_HW);

	k_sleep(K_MSEC(500));

	/* Example 7: Ukrainian Dorian Scale */
	LOG_INF("Ukrainian Dorian scale:");
	generate_chord(60, HARM_MASK_DORIAN_S4);

	k_sleep(K_MSEC(500));

	/* Example 8: Chord Recognition from arbitrary notes */
	uint8_t notes[] = { 64, 67, 72 }; /* E, G, C */
	char chord_name[64];
	harm_recognize_chord_from_notes(notes, 3, chord_name, sizeof(chord_name));
	LOG_INF("Notes {64, 67, 72} recognized as: %s", chord_name);

	uint8_t notes2[] = { 57, 60, 64, 67 }; /* A, C, E, G (Am7) */
	harm_recognize_chord_from_notes(notes2, 4, chord_name, sizeof(chord_name));
	LOG_INF("Notes {57, 60, 64, 67} recognized as: %s", chord_name);

	LOG_INF("Sample execution complete.");
	return 0;
}
