/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file note.c
 *
 * @brief Generic MIDI and harmony related functions
 * implemented in support of embedded systems in c.
 * tested on Zephyr RTOS.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20241224
 */
#include <zephyr/midi1/note.h>

/*
 * Return the note with the octave included.
 */
const char *harm_note_to_text_with_octave(uint8_t midinote, bool flats)
{
	static char notestring[8];
	snprintf(notestring, sizeof(notestring), "%s%d", harm_note_to_text(midinote, flats),
		 harm_note_to_oct(midinote));
	return &notestring[0];
}

/* This function converts a MIDI note using a lookup table to a string */
const char *harm_note_to_text(uint8_t midinote, bool flats)
{
	int octave = harm_note_to_oct(midinote);
	uint8_t note = midinote - ((octave + 2) * 12);
	static const char *flatNotes[] = {"C ", "Db", "D ", "Eb", "E ", "F ",
					  "Gb", "G ", "Ab", "A ", "Bb", "B "};
	static const char *sharpNotes[] = {"C ", "C#", "D ", "D#", "E ", "F ",
					   "F#", "G ", "G#", "A ", "A#", "B "};

	const char *noteString;
	if (flats) {
		noteString = flatNotes[note];
	} else {
		noteString = sharpNotes[note];
	}
	/*  Null-terminated string is returned from the static char array */
	return noteString;
}

/*
 * This function converts a MIDI note to an octave number
 */
int harm_note_to_oct(uint8_t midinote)
{
	return (midinote / 12) - 2;
}

/**
 * In this case on my embedded system I don't want to run the
 * pow() match functions I use a precomputed lookup table.
 * A=440Hz is a given then.
 */
float harm_note_to_freq(uint8_t midinote)
{
	if (midinote > 127) {
		return 0.0f;
	}
	return midi_freq_table[midinote];
}

/**
 * @brief Convert a frequency value to the nearest MIDI note number.
 *
 * This function uses a binary search over the precomputed MIDI note
 * frequency table (0–127).
 *
 * @param freq  Input frequency in Hz
 * @return      MIDI note number (0–127) that best matches the frequency
 */
uint8_t harm_freq_to_midi_note(float freq)
{
	int min = 0;
	int max = 127;

	while (min <= max) {
		int mid = (min + max) / 2;

		if (freq > midi_freq_table[mid]) {
			min = mid + 1;
		} else if (freq < midi_freq_table[mid]) {
			max = mid - 1;
		} else {
			/* exact match */
			return mid;
		}
	}

	/*  min is now the first note with freq_table[min] > freq */
	if (min == 0) {
		return 0;
	}
	if (min > 127) {
		return 127;
	}

	/* pick closest of min and min-1 */
	float low = midi_freq_table[min - 1];
	float high = midi_freq_table[min];

	return (freq - low < high - freq) ? (min - 1) : min;
}
