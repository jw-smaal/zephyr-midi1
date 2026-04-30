/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file harmonic.c
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260429
 */

#include <zephyr/midi1/harmonic.h>

static const char *note_names_sharps[] = {
	"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static const char *note_names_flats[] = {
	"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

#define NOTES_PER_OCTAVE 12u
#define MASK_12BIT_LIMIT 0x0FFFu

bool harmonic_is_note_in_mask(uint8_t note, harmonic_mask_t mask, uint8_t root)
{
	uint8_t note_in_octave = note % NOTES_PER_OCTAVE;
	int8_t relative_note = (int8_t)note_in_octave - (int8_t)(root % NOTES_PER_OCTAVE);

	if (relative_note < 0) {
		relative_note += (int8_t)NOTES_PER_OCTAVE;
	}

	return (mask & (1u << (uint8_t)relative_note)) != 0;
}

harmonic_mask_t harmonic_transpose_mask(harmonic_mask_t mask, uint8_t semitones)
{
	uint8_t shift = semitones % NOTES_PER_OCTAVE;
	if (shift == 0) {
		return mask;
	}

	/* Rotate bits: (mask << shift) | (mask >> (12 - shift)) */
	harmonic_mask_t high = (mask << shift) & MASK_12BIT_LIMIT;
	harmonic_mask_t low = (mask >> (NOTES_PER_OCTAVE - shift)) & MASK_12BIT_LIMIT;
	
	return (high | low);
}

harmonic_mask_t harmonic_rotate_mask(harmonic_mask_t mask, uint8_t new_root_index)
{
	uint8_t count = 0;
	uint8_t shift = 0;
	bool found = false;

	for (uint8_t i = 0; i < NOTES_PER_OCTAVE; i++) {
		if (mask & (1u << i)) {
			if (count == new_root_index) {
				shift = i;
				found = true;
				break;
			}
			count++;
		}
	}

	if (!found || shift == 0) {
		return mask;
	}

	/* Rotate so bit 'shift' becomes bit 0 */
	harmonic_mask_t low = (mask >> shift) & MASK_12BIT_LIMIT;
	harmonic_mask_t high = (mask << (NOTES_PER_OCTAVE - shift)) & MASK_12BIT_LIMIT;

	return (low | high);
}

const char *harmonic_get_note_name(uint8_t note, bool use_sharps)
{
	uint8_t note_in_octave = note % NOTES_PER_OCTAVE;
	if (use_sharps) {
		return note_names_sharps[note_in_octave];
	} else {
		return note_names_flats[note_in_octave];
	}
}

uint8_t harmonic_get_mask_note_count(harmonic_mask_t mask)
{
	uint8_t count = 0;
	harmonic_mask_t m = mask & MASK_12BIT_LIMIT;

	while (m != 0) {
		m &= (m - 1);
		count++;
	}

	return count;
}
