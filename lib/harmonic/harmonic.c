/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file harmonic.c
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260429
 */

#include <zephyr/midi1/harmonic.h>
#include <stdio.h>
#include <string.h>

static const char *note_names_sharps[] = {
	"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

static const char *note_names_flats[] = {
	"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

struct chord_entry {
	harm_mask_t mask;
	const char *name;
};

static const struct chord_entry common_chords[] = {
	{ HARM_MASK_MAJOR_TRIAD,      "Major Triad" },
	{ HARM_MASK_MINOR_TRIAD,      "Minor Triad" },
	{ HARM_MASK_DIMINISHED_TRIAD, "Diminished Triad" },
	{ HARM_MASK_AUGMENTED_TRIAD,  "Augmented Triad" },
	{ HARM_MASK_MAJOR_7TH,        "Major 7th" },
	{ HARM_MASK_MINOR_7TH,        "Minor 7th" },
	{ HARM_MASK_DOMINANT_7TH,     "Dominant 7th" },
	{ HARM_MASK_DIMINISHED_7TH,   "Diminished 7th" },
	{ HARM_MASK_HALF_DIM_7TH,     "Half-Diminished 7th" },
	{ HARM_MASK_MINOR_MAJOR_7TH,  "Minor-Major 7th" },
	{ HARM_MASK_MAJOR_6TH,        "Major 6th" },
	{ HARM_MASK_MINOR_6TH,        "Minor 6th" },
	{ HARM_MASK_MAJOR_9TH,        "Major 9th" },
	{ HARM_MASK_MINOR_9TH,        "Minor 9th" },
	{ HARM_MASK_DOMINANT_9TH,     "Dominant 9th" },
	{ HARM_MASK_DOMINANT_11TH,    "Dominant 11th" },
	{ HARM_MASK_DOMINANT_13TH,    "Dominant 13th" },
	{ HARM_MASK_SUS2,             "Sus2" },
	{ HARM_MASK_SUS4,             "Sus4" }
};

#define NOTES_PER_OCTAVE 12u
#define MASK_12BIT_LIMIT 0x0FFFu

bool harm_is_note_in_mask(uint8_t note, harm_mask_t mask, uint8_t root)
{
	uint8_t note_in_octave = note % NOTES_PER_OCTAVE;
	int8_t relative_note = (int8_t)note_in_octave - (int8_t)(root % NOTES_PER_OCTAVE);

	if (relative_note < 0) {
		relative_note += (int8_t)NOTES_PER_OCTAVE;
	}

	return (mask & (1u << (uint8_t)relative_note)) != 0;
}

harm_mask_t harm_transpose_mask(harm_mask_t mask, uint8_t semitones)
{
	uint8_t shift = semitones % NOTES_PER_OCTAVE;
	if (shift == 0) {
		return mask;
	}

	/* Rotate bits: (mask << shift) | (mask >> (12 - shift)) */
	harm_mask_t high = (mask << shift) & MASK_12BIT_LIMIT;
	harm_mask_t low = (mask >> (NOTES_PER_OCTAVE - shift)) & MASK_12BIT_LIMIT;
	
	return (high | low);
}

harm_mask_t harm_rotate_mask(harm_mask_t mask, uint8_t new_root_index)
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
	harm_mask_t low = (mask >> shift) & MASK_12BIT_LIMIT;
	harm_mask_t high = (mask << (NOTES_PER_OCTAVE - shift)) & MASK_12BIT_LIMIT;

	return (low | high);
}

const char *harm_get_note_name(uint8_t note, bool use_sharps)
{
	uint8_t note_in_octave = note % NOTES_PER_OCTAVE;
	if (use_sharps) {
		return note_names_sharps[note_in_octave];
	} else {
		return note_names_flats[note_in_octave];
	}
}

uint8_t harm_get_mask_note_count(harm_mask_t mask)
{
	uint8_t count = 0;
	harm_mask_t m = mask & MASK_12BIT_LIMIT;

	while (m != 0) {
		m &= (m - 1);
		count++;
	}

	return count;
}

uint8_t harm_get_distinct_mode_count(harm_mask_t mask)
{
	uint8_t note_count = harm_get_mask_note_count(mask);
	if (note_count == 0) {
		return 0;
	}

	uint8_t distinct_count = 0;
	harm_mask_t seen_masks[NOTES_PER_OCTAVE];

	for (uint8_t i = 0; i < note_count; i++) {
		harm_mask_t rotated = harm_rotate_mask(mask, i);
		bool found = false;

		for (uint8_t j = 0; j < distinct_count; j++) {
			if (seen_masks[j] == rotated) {
				found = true;
				break;
			}
		}

		if (!found) {
			seen_masks[distinct_count++] = rotated;
		}
	}

	return distinct_count;
}

const char *harm_recognize_chord(harm_mask_t mask)
{
	harm_mask_t m = mask & MASK_12BIT_LIMIT;

	for (size_t i = 0; i < sizeof(common_chords) / sizeof(struct chord_entry); i++) {
		if (common_chords[i].mask == m) {
			return common_chords[i].name;
		}
	}

	return "Unknown";
}

int harm_recognize_chord_from_notes(const uint8_t *notes, uint8_t count, 
                                    char *name_buf, size_t buf_len)
{
	if (count == 0 || name_buf == NULL || buf_len == 0) {
		return -1;
	}

	uint8_t lowest_note = 255;
	bool present_notes[NOTES_PER_OCTAVE] = { false };

	for (uint8_t i = 0; i < count; i++) {
		if (notes[i] < lowest_note) {
			lowest_note = notes[i];
		}
		present_notes[notes[i] % NOTES_PER_OCTAVE] = true;
	}

	for (uint8_t r = 0; r < NOTES_PER_OCTAVE; r++) {
		if (!present_notes[r]) {
			continue;
		}

		/* Test this note as the root */
		harm_mask_t test_mask = 0;
		for (uint8_t n = 0; n < NOTES_PER_OCTAVE; n++) {
			if (present_notes[n]) {
				int8_t rel = (int8_t)n - (int8_t)r;
				if (rel < 0) rel += NOTES_PER_OCTAVE;
				test_mask |= (1u << rel);
			}
		}

		const char *chord_type = harm_recognize_chord(test_mask);
		if (strcmp(chord_type, "Unknown") != 0) {
			const char *root_name = harm_get_note_name(r, true);
			if (r == (lowest_note % NOTES_PER_OCTAVE)) {
				snprintf(name_buf, buf_len, "%s %s", root_name, chord_type);
			} else {
				const char *bass_name = harm_get_note_name(lowest_note % NOTES_PER_OCTAVE, true);
				snprintf(name_buf, buf_len, "%s %s / %s", root_name, chord_type, bass_name);
			}
			return 0;
		}
	}

	snprintf(name_buf, buf_len, "Unknown Chord");
	return 0;
}
