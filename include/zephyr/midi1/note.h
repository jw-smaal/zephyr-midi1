/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file note.h
 *
 * @brief generic MIDI and harmony related functions
 * implemented in support of embedded systems in c.
 * tested on Zephyr RTOS.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20241224
 */
#ifndef ZEPHYR_MIDI1_NOTE_H_
#define ZEPHYR_MIDI1_NOTE_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * Pre computed values for the note frequencies
 */
#include <zephyr/midi1/midi_freq_table.h>

/* 440 Hz for the A4 note */
#define BASE_A4_NOTE_FREQUENCY 440

/**
 * @brief converts a MIDI note using a lookup table to a string
 *
 * @param midinote in MIDI format (limited to 0 -> 127)
 * @param flats represent not in flats or sharps (default)
 * @return char note in text format e.g. Db
 */
const char *harm_note_to_text(uint8_t midinote, bool flats);

/**
 * @brief convert note to text with octave
 * @param midinote in MIDI format (limited to 0 -> 127)
 * @return char note in text format e.g. Db3
 */
const char *harm_note_to_text_with_octave(uint8_t midinote, bool flats);

/**
 * @brief note to octave
 * @param midinote in MIDI format (limited to 0 -> 127)
 * @return octave e.g. -3 or 4
 */
int harm_note_to_oct(uint8_t midinote);

/**
 * @brief converts note to frequency using lookup table
 * @param midinote in MIDI format (limited to 0 -> 127)
 * @return frequency referenceded to A440
 */
float harm_note_to_freq(uint8_t midinote);

/**
 * @brief converts frequency to MIDI note approximate
 * @param frequency referenceded to A440
 * @return midinote in MIDI format (limited to 0 -> 127)
 */
uint8_t harm_freq_to_midi_note(float freq);

#endif /* ZEPHYR_MIDI1_NOTE_H_ */
