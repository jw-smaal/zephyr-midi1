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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Note definitions (0-11)
 */
enum harmonic_note {
	HARMONIC_NOTE_C  = 0,
	HARMONIC_NOTE_CS = 1,
	HARMONIC_NOTE_DB = 1,
	HARMONIC_NOTE_D  = 2,
	HARMONIC_NOTE_DS = 3,
	HARMONIC_NOTE_EB = 3,
	HARMONIC_NOTE_E  = 4,
	HARMONIC_NOTE_F  = 5,
	HARMONIC_NOTE_FS = 6,
	HARMONIC_NOTE_GB = 6,
	HARMONIC_NOTE_G  = 7,
	HARMONIC_NOTE_GS = 8,
	HARMONIC_NOTE_AB = 8,
	HARMONIC_NOTE_A  = 9,
	HARMONIC_NOTE_AS = 10,
	HARMONIC_NOTE_BB = 10,
	HARMONIC_NOTE_B  = 11,
	HARMONIC_NOTE_COUNT = 12
};

/**
 * @brief Interval definitions (semitones from root)
 */
enum harmonic_interval {
	HARMONIC_INTERVAL_UNISON        = 0,
	HARMONIC_INTERVAL_MINOR_2ND     = 1,
	HARMONIC_INTERVAL_MAJOR_2ND     = 2,
	HARMONIC_INTERVAL_MINOR_3RD     = 3,
	HARMONIC_INTERVAL_MAJOR_3RD     = 4,
	HARMONIC_INTERVAL_PERFECT_4TH   = 5,
	HARMONIC_INTERVAL_TRITONE       = 6,
	HARMONIC_INTERVAL_PERFECT_5TH   = 7,
	HARMONIC_INTERVAL_MINOR_6TH     = 8,
	HARMONIC_INTERVAL_MAJOR_6TH     = 9,
	HARMONIC_INTERVAL_MINOR_7TH     = 10,
	HARMONIC_INTERVAL_MAJOR_7TH     = 11,
	HARMONIC_INTERVAL_OCTAVE        = 12
};

/**
 * @brief Harmonic mask type (12 bits)
 * Bit 0 = Root, Bit 1 = m2, Bit 2 = M2, ..., Bit 11 = M7
 */
typedef uint16_t harmonic_mask_t;

/**
 * @brief Common scale masks using binary literals for clarity.
 * Format: 0b[B][Bb][A][Ab][G][Gb][F][E][Eb][D][Db][C]
 */
enum harmonic_scale_mask {
	HARMONIC_MASK_MAJOR            = 0b101010110101,
	HARMONIC_MASK_NATURAL_MINOR    = 0b010110101101,
	HARMONIC_MASK_HARMONIC_MINOR   = 0b100110101101,
	HARMONIC_MASK_MELODIC_MINOR    = 0b101010101101,
	HARMONIC_MASK_DORIAN           = 0b010110110101,
	HARMONIC_MASK_PHRYGIAN         = 0b010101101101,
	HARMONIC_MASK_LYDIAN           = 0b101011110101,
	HARMONIC_MASK_MIXOLYDIAN       = 0b011010110101,
	HARMONIC_MASK_LOCRIAN          = 0b010101101011,
	HARMONIC_MASK_PENTATONIC_MAJOR = 0b001010010101,
	HARMONIC_MASK_PENTATONIC_MINOR = 0b010010101001,
	HARMONIC_MASK_BLUES            = 0b010011101101,
	HARMONIC_MASK_OCTATONIC_HW     = 0b011011011011,
	HARMONIC_MASK_OCTATONIC_WH     = 0b101101101101,
	HARMONIC_MASK_GYPSY            = 0b100110101101,
	HARMONIC_MASK_SYMETRICAL       = 0b001011101101,
	HARMONIC_MASK_ENIGMATIC        = 0b100011110011,
	HARMONIC_MASK_ARABIAN          = 0b111010110001,
	HARMONIC_MASK_HUNGARIAN        = 0b100100101101,
	HARMONIC_MASK_WHOLE_TONE       = 0b010101010101,
	HARMONIC_MASK_AUGMENTED        = 0b100100100101,
	HARMONIC_MASK_CHROMATIC        = 0b111111111111
};

/**
 * @brief Common chord masks using binary literals.
 */
enum harmonic_chord_mask {
	HARMONIC_MASK_MAJOR_TRIAD      = 0b000010010001,
	HARMONIC_MASK_MINOR_TRIAD      = 0b000010001001,
	HARMONIC_MASK_DIMINISHED_TRIAD = 0b000001001001,
	HARMONIC_MASK_AUGMENTED_TRIAD  = 0b000100010001,
	HARMONIC_MASK_MAJOR_7TH        = 0b100010010001,
	HARMONIC_MASK_MINOR_7TH        = 0b010010001001,
	HARMONIC_MASK_DOMINANT_7TH     = 0b010010010001,
	HARMONIC_MASK_DIMINISHED_7TH   = 0b001001001001,
	HARMONIC_MASK_HALF_DIM_7TH     = 0b010001001001,
	HARMONIC_MASK_MINOR_MAJOR_7TH  = 0b100010001001,
	HARMONIC_MASK_SUS2             = 0b000010000101,
	HARMONIC_MASK_SUS4             = 0b000010100001
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
bool harmonic_is_note_in_mask(uint8_t note, harmonic_mask_t mask, uint8_t root);

/**
 * @brief Transpose a mask
 *
 * @param mask Original mask
 * @param semitones Number of semitones to transpose (0-11)
 *
 * @return Transposed mask
 */
harmonic_mask_t harmonic_transpose_mask(harmonic_mask_t mask, uint8_t semitones);

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
harmonic_mask_t harmonic_rotate_mask(harmonic_mask_t mask, uint8_t new_root_index);

/**
 * @brief Get the name of a note
 *
 * @param note Note number (0-11)
 * @param use_sharps True for sharps, false for flats
 *
 * @return Note name string (e.g., "C", "C#", "Db")
 */
const char *harmonic_get_note_name(uint8_t note, bool use_sharps);

/**
 * @brief Get the number of notes in a mask
 *
 * @param mask Harmonic mask
 *
 * @return Number of set bits
 */
uint8_t harmonic_get_mask_note_count(harmonic_mask_t mask);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_MIDI1_HARMONIC_H_ */
