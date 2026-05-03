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
	HARM_NOTE_C  = 0,
	HARM_NOTE_Cs = 1,
	HARM_NOTE_Db = 1,
	HARM_NOTE_D  = 2,
	HARM_NOTE_Ds = 3,
	HARM_NOTE_Eb = 3,
	HARM_NOTE_E  = 4,
	HARM_NOTE_F  = 5,
	HARM_NOTE_Fs = 6,
	HARM_NOTE_Gb = 6,
	HARM_NOTE_G  = 7,
	HARM_NOTE_Gs = 8,
	HARM_NOTE_Ab = 8,
	HARM_NOTE_A  = 9,
	HARM_NOTE_As = 10,
	HARM_NOTE_Bb = 10,
	HARM_NOTE_B  = 11,
	HARM_NOTE_COUNT = 12
};

/**
 * @brief Interval definitions (semitones from root)
 */
enum harm_interval {
	HARM_INT_UNISON        = 0,
	HARM_INT_MINOR_2ND     = 1,
	HARM_INT_MAJOR_2ND     = 2,
	HARM_INT_MINOR_3RD     = 3,
	HARM_INT_MAJOR_3RD     = 4,
	HARM_INT_PERFECT_4TH   = 5,
	HARM_INT_TRITONE       = 6,
	HARM_INT_PERFECT_5TH   = 7,
	HARM_INT_MINOR_6TH     = 8,
	HARM_INT_MAJOR_6TH     = 9,
	HARM_INT_MINOR_7TH     = 10,
	HARM_INT_MAJOR_7TH     = 11,
	HARM_INT_OCTAVE        = 12
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
	HARM_MASK_MAJOR            = 0b101010110101,
	HARM_MASK_NATURAL_MINOR    = 0b010110101101,
	HARM_MASK_HARMONIC_MINOR   = 0b100110101101,
	HARM_MASK_MELODIC_MINOR    = 0b101010101101,
	HARM_MASK_DORIAN           = 0b010110110101,
	HARM_MASK_PHRYGIAN         = 0b010101101101,
	HARM_MASK_LYDIAN           = 0b101011110101,
	HARM_MASK_MIXOLYDIAN       = 0b011010110101,
	HARM_MASK_LOCRIAN          = 0b010101101011,
	HARM_MASK_PENTATONIC_MAJOR = 0b001010010101,
	HARM_MASK_PENTATONIC_MINOR = 0b010010101001,
	HARM_MASK_BLUES            = 0b010011101101,
	HARM_MASK_OCTATONIC_HW     = 0b011011011011,
	HARM_MASK_OCTATONIC_WH     = 0b101101101101,
	HARM_MASK_GYPSY            = 0b100110101101,
	HARM_MASK_SYMETRICAL       = 0b001011101101,
	HARM_MASK_ENIGMATIC        = 0b100011110011,
	HARM_MASK_ARABIAN          = 0b111010110001,
	HARM_MASK_HUNGARIAN        = 0b100100101101,
	HARM_MASK_WHOLE_TONE       = 0b010101010101,
	HARM_MASK_AUGMENTED        = 0b100100100101,
	HARM_MASK_CHROMATIC        = 0b111111111111
};

/**
 * @brief Common chord masks using binary literals.
 */
enum harm_chord_mask {
	HARM_MASK_MAJOR_TRIAD      = 0b000010010001,
	HARM_MASK_MINOR_TRIAD      = 0b000010001001,
	HARM_MASK_DIMINISHED_TRIAD = 0b000001001001,
	HARM_MASK_AUGMENTED_TRIAD  = 0b000100010001,
	HARM_MASK_MAJOR_7TH        = 0b100010010001,
	HARM_MASK_MINOR_7TH        = 0b010010001001,
	HARM_MASK_DOMINANT_7TH     = 0b010010010001,
	HARM_MASK_DIMINISHED_7TH   = 0b001001001001,
	HARM_MASK_HALF_DIM_7TH     = 0b010001001001,
	HARM_MASK_MINOR_MAJOR_7TH  = 0b100010001001,
	HARM_MASK_SUS2             = 0b000010000101,
	HARM_MASK_SUS4             = 0b000010100001
};

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

#endif /* ZEPHYR_MIDI1_HARMONIC_H_ */
