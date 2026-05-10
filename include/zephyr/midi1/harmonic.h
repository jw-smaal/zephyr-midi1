/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file harmonic.h
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260429
 */

#ifndef ZEPHYR_MIDI1_HARMONIC_H_
#define ZEPHYR_MIDI1_HARMONIC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Note definitions (0-11)
 */
enum harm_note {
	HARM_NOTE_C = 0,
	HARM_NOTE_Cs = 1,
	HARM_NOTE_Db = 1,
	HARM_NOTE_D = 2,
	HARM_NOTE_Ds = 3,
	HARM_NOTE_Eb = 3,
	HARM_NOTE_E = 4,
	HARM_NOTE_F = 5,
	HARM_NOTE_Fs = 6,
	HARM_NOTE_Gb = 6,
	HARM_NOTE_G = 7,
	HARM_NOTE_Gs = 8,
	HARM_NOTE_Ab = 8,
	HARM_NOTE_A = 9,
	HARM_NOTE_As = 10,
	HARM_NOTE_Bb = 10,
	HARM_NOTE_B = 11,
	HARM_NOTE_COUNT = 12
};

/**
 * @brief Interval definitions (semitones from root)
 */
enum harm_interval {
	HARM_INT_UNISON = 0,
	HARM_INT_MINOR_2ND = 1,
	HARM_INT_MAJOR_2ND = 2,
	HARM_INT_MINOR_3RD = 3,
	HARM_INT_MAJOR_3RD = 4,
	HARM_INT_PERFECT_4TH = 5,
	HARM_INT_TRITONE = 6,
	HARM_INT_PERFECT_5TH = 7,
	HARM_INT_MINOR_6TH = 8,
	HARM_INT_MAJOR_6TH = 9,
	HARM_INT_MINOR_7TH = 10,
	HARM_INT_MAJOR_7TH = 11,
	HARM_INT_OCTAVE = 12
};

/**
 * @brief Harmonic mask type (12 bits)
 * Bit 0 = Root, Bit 1 = m2, Bit 2 = M2, ..., Bit 11 = M7
 */
typedef uint16_t harm_mask_t;

/**
 * @brief Common scale masks using binary literals for clarity.
 * Format: 0b[B][Bb][A][Ab][G][Gb][F][E][Eb][D][Db][C]
 */
enum harm_scale_mask {
	/* Modes of the Major Scale */
	HARM_MASK_MAJOR = 0b101010110101, /* Ionian */
	HARM_MASK_DORIAN = 0b010110110101,
	HARM_MASK_PHRYGIAN = 0b010101101101,
	HARM_MASK_LYDIAN = 0b101011110101,
	HARM_MASK_MIXOLYDIAN = 0b011010110101,
	HARM_MASK_NATURAL_MINOR = 0b010110101101, /* Aeolian */
	HARM_MASK_LOCRIAN = 0b010101101011,

	/* Modes of Melodic Minor */
	HARM_MASK_MELODIC_MINOR = 0b101010101101,
	HARM_MASK_DORIAN_B2 = 0b011001101011,
	HARM_MASK_LYDIAN_AUG = 0b101101010101,
	HARM_MASK_LYDIAN_DOM = 0b011011010101,
	HARM_MASK_MIXOLYDIAN_B6 = 0b010110110101,
	HARM_MASK_LOCRIAN_S2 = 0b010101101101,
	HARM_MASK_ALTERED = 0b010101010111,

	/* Modes of Harmonic Minor */
	HARM_MASK_HARMONIC_MINOR = 0b100110101101,
	HARM_MASK_LOCRIAN_S6 = 0b011001101101,
	HARM_MASK_IONIAN_S5 = 0b101100110101,
	HARM_MASK_DORIAN_S4 = 0b011011001101, /* Ukrainian Dorian */
	HARM_MASK_UKRAINIAN_DORIAN = 0b011011001101,
	HARM_MASK_PHRYGIAN_DOM = 0b010100110011,
	HARM_MASK_LYDIAN_S2 = 0b101011011001,
	HARM_MASK_ULTRALOCRIAN = 0b001101010111,

	/* Symmetrical and Pentatonic Scales */
	HARM_MASK_PENTATONIC_MAJOR = 0b001010010101,
	HARM_MASK_PENTATONIC_MINOR = 0b010010101001,
	HARM_MASK_BLUES = 0b010011101101,
	HARM_MASK_OCTATONIC_HW = 0b011011011011,
	HARM_MASK_OCTATONIC_WH = 0b101101101101,
	HARM_MASK_WHOLE_TONE = 0b010101010101,
	HARM_MASK_AUGMENTED = 0b100100100101,
	HARM_MASK_CHROMATIC = 0b111111111111,

	/* World Scales */
	HARM_MASK_DOUBLE_HARMONIC = 0b100100110011,
	HARM_MASK_GYPSY = 0b100110101101,
	HARM_MASK_HUNGARIAN_MINOR = 0b100111001101,
	HARM_MASK_HUNGARIAN_MAJOR = 0b011011011001,
	HARM_MASK_ARABIAN = 0b111010110001,
	HARM_MASK_PERSIAN = 0b100101110011,
	HARM_MASK_ENIGMATIC = 0b100011110011,
	HARM_MASK_SYMETRICAL = 0b001011101101,
	HARM_MASK_HIRAJOSHI = 0b100011010001,
	HARM_MASK_INSEN = 0b010010100011,

	/* Persichetti Synthetic Scales */
	HARM_MASK_PROMETHEUS = 0b011001010101,
	HARM_MASK_NEAPOLITAN_MAJOR = 0b101010101011,
	HARM_MASK_NEAPOLITAN_MINOR = 0b100110101011
};

/**
 * @brief Common chord masks using binary literals.
 */
enum harm_chord_mask {
	HARM_MASK_MAJOR_TRIAD = 0b000010010001,
	HARM_MASK_MINOR_TRIAD = 0b000010001001,
	HARM_MASK_DIMINISHED_TRIAD = 0b000001001001,
	HARM_MASK_AUGMENTED_TRIAD = 0b000100010001,
	HARM_MASK_MAJOR_7TH = 0b100010010001,
	HARM_MASK_MINOR_7TH = 0b010010001001,
	HARM_MASK_DOMINANT_7TH = 0b010010010001,
	HARM_MASK_DIMINISHED_7TH = 0b001001001001,
	HARM_MASK_HALF_DIM_7TH = 0b010001001001,
	HARM_MASK_MINOR_MAJOR_7TH = 0b100010001001,
	HARM_MASK_MAJOR_6TH = 0b001010010001,
	HARM_MASK_MINOR_6TH = 0b001010001001,
	HARM_MASK_MAJOR_ADD9 = 0b000010010101,
	HARM_MASK_MINOR_ADD9 = 0b000010001101,
	HARM_MASK_MAJOR_ADD11 = 0b000010110001,
	HARM_MASK_MINOR_ADD11 = 0b000010101001,
	HARM_MASK_MAJOR_6_9 = 0b001010010101,
	HARM_MASK_MINOR_6_9 = 0b001010001101,
	HARM_MASK_DOM_7_B9 = 0b010010010011,
	HARM_MASK_DOM_7_S9 = 0b010010011001,
	HARM_MASK_DOM_7_SUS4 = 0b010010100001,
	HARM_MASK_MAJ_7_SUS4 = 0b100010100001,
	HARM_MASK_MAJOR_9TH = 0b100010010101,
	HARM_MASK_MINOR_9TH = 0b010010001101,
	HARM_MASK_DOMINANT_9TH = 0b010010010101,
	HARM_MASK_DOMINANT_11TH = 0b010010110101,
	HARM_MASK_DOMINANT_13TH = 0b011010110101,
	HARM_MASK_SUS2 = 0b000010000101,
	HARM_MASK_SUS4 = 0b000010100001,
	HARM_MASK_POWER_CHORD = 0b000010000001,

	/* 20th Century Quartal & Secundal Chords */
	HARM_MASK_QUARTAL_3 = 0b010000100001,        /* 0-5-10 (C-F-Bb) */
	HARM_MASK_QUARTAL_4 = 0b010000101001,        /* 0-5-10-3 (C-F-Bb-Eb) */
	HARM_MASK_SECUNDAL_MAJ_MAJ = 0b000000010101, /* 0-2-4 (C-D-E) */
	HARM_MASK_SECUNDAL_MAJ_MIN = 0b000000001101, /* 0-2-3 (C-D-Eb) */

	/* Jazz Extensions and Persichetti Added-Note Chords */
	HARM_MASK_MAJOR_ADD_S4 = 0b000011010001, /* Major add #4 (0-4-6-7) */
	HARM_MASK_MAJOR_ADD_B2 = 0b000010010011, /* Major add b2 (0-1-4-7) */
	HARM_MASK_MINOR_ADD_B2 = 0b000010001011, /* Minor add b2 (0-1-3-7) */
	HARM_MASK_MAJOR_ADD_B6 = 0b000110010001, /* Major add b6 (0-4-7-8) */
	HARM_MASK_DOM_7_S11 = 0b010011010001,    /* 7(#11) (0-4-6-7-10) */
	HARM_MASK_MAJ_7_S11 = 0b100011010001,    /* Maj7(#11) (0-4-6-7-11) */
	HARM_MASK_DOM_7_B13 = 0b010110010001,    /* 7(b13) (0-4-7-8-10) */
	HARM_MASK_DOM_7_AUG = 0b010100010001,    /* 7(#5) (0-4-8-10) */
	HARM_MASK_MINOR_7_B9 = 0b010010001011,   /* m7(b9) (0-1-3-7-10) */

	/* Missing Jazz & Modern Extensions */
	HARM_MASK_DOM_7_B5 = 0b010001010001,          /* 7(b5) (0-4-6-10) */
	HARM_MASK_MAJ_7_B5 = 0b100001010001,          /* Maj7(b5) (0-4-6-11) */
	HARM_MASK_MINOR_11TH = 0b010010101101,        /* m11 (0-2-3-5-7-10) */
	HARM_MASK_MAJOR_11TH = 0b100010110101,        /* Maj11 (0-2-4-5-7-11) */
	HARM_MASK_DOM_9_SUS4 = 0b010010100101,        /* 9sus4 (0-2-5-7-10) */
	HARM_MASK_DOM_13_SUS4 = 0b011010100101,       /* 13sus4 (0-2-5-7-9-10) */
	HARM_MASK_PHRYGIAN_DOM_CHORD = 0b010010100011 /* 7sus4(b9) (0-1-5-7-10) */
};

/**
 * @brief Check if a harmonic mask represents a cluster (3+ contiguous bits)
 *
 * @param mask Harmonic mask
 * @return true if it is a cluster
 */
bool harm_is_cluster(harm_mask_t mask);

/**
 * @brief Check if a note is in a mask
 *
 * @param note Note number (0-127)
 * @param mask Harmonic mask
 * @param root Root note of the mask (0-11)
 *
 * @return true if the note is in the mask
 */
bool harm_is_note_in_mask(uint8_t note, harm_mask_t mask, uint8_t root);

/**
 * @brief Transpose a mask
 *
 * @param mask Original mask
 * @param semitones Number of semitones to transpose (0-11)
 *
 * @return Transposed mask
 */
harm_mask_t harm_transpose_mask(harm_mask_t mask, uint8_t semitones);

/**
 * @brief Rotate a mask to a new root within the mask
 *
 * Useful for finding modes of a scale.
 *
 * @param mask Original mask
 * @param new_root_index The index of the new root among the set bits (0-based)
 *
 * @return Rotated mask where the new root is at bit 0
 */
harm_mask_t harm_rotate_mask(harm_mask_t mask, uint8_t new_root_index);

/**
 * @brief Get the name of a note
 *
 * @param note Note number (0-11)
 * @param use_sharps True for sharps, false for flats
 *
 * @return Note name string (e.g., "C", "C#", "Db")
 */
const char *harm_get_note_name(uint8_t note, bool use_sharps);

/**
 * @brief Get the number of notes in a mask
 *
 * @param mask Harmonic mask
 *
 * @return Number of set bits
 */
uint8_t harm_get_mask_note_count(harm_mask_t mask);

/**
 * @brief Get the number of distinct modes for a scale
 *
 * This calculates how many unique rotations of the scale exist before
 * it repeats. For example, the Major scale has 7 distinct modes, while
 * the Whole Tone scale only has 1.
 *
 * @param mask Harmonic mask representing a scale
 *
 * @return Number of distinct modes (1 to note_count)
 */
uint8_t harm_get_distinct_mode_count(harm_mask_t mask);

/**
 * @brief Recognize a chord from a harmonic mask
 *
 * @param mask Harmonic mask
 *
 * @return Name of the recognized chord, or "Unknown"
 */
const char *harm_recognize_chord(harm_mask_t mask);

/**
 * @brief Recognize a chord from a set of MIDI notes
 *
 * This function attempts to identify the root note and the chord type
 * from an arbitrary set of notes. It also detects inversions.
 *
 * @param notes Array of MIDI note numbers
 * @param count Number of notes
 * @param name_buf Buffer to store the resulting name (e.g., "C Major / G")
 * @param buf_len Length of the buffer
 *
 * @return 0 on success, negative error code otherwise
 */
int harm_recognize_chord_from_notes(const uint8_t *notes, uint8_t count, char *name_buf,
				    size_t buf_len);

/**
 * @brief Information about a recognized scale
 */
struct harm_scale_result {
	const char *name;
	uint8_t root;
	uint8_t score; /* 0-100 percentage match */
};

/**
 * @brief Recognize the most likely scales from a harmonic mask
 *
 * @param mask Harmonic mask (representing a set of notes played over time)
 * @param results Array to store the top results
 * @param max_results Maximum number of results to return
 *
 * @return Number of results found
 */
int harm_recognize_scale(harm_mask_t mask, struct harm_scale_result *results, uint8_t max_results);

#endif /* ZEPHYR_MIDI1_HARMONIC_H_ */
