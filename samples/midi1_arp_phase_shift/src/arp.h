/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ARP_H_
#define ARP_H_

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * @file arp.h
 * @brief Modular Arpeggiator Logic for Phasing and Process-based Counter-point
 */

#define ARP_MAX_NOTES 16

/**
 * @brief Arpeggiator operating modes.
 */
enum arp_mode {
	ARP_MODE_SYNC = 0,          /* Fixed sequence, synchronized layers */
	ARP_MODE_PHASE = 1,         /* Fixed sequence, drifting layers */
	ARP_MODE_PROCESS = 2,       /* Evolving sequence, synchronized layers */
	ARP_MODE_PHASE_PROCESS = 3, /* Evolving sequence, drifting layers */
	ARP_MODE_MAX = 4
};

struct arp_note {
	uint8_t pitch;
	uint8_t velocity;
};

struct arp_layer {
	uint8_t playing_pitch;
	int index;
	int clock_counter;
	int drift_trigger_count;
	bool lag_pending;
};

struct arp_ctx {
	struct arp_note notes[ARP_MAX_NOTES];
	int num_notes;

	struct arp_layer base;
	struct arp_layer high;

	/* Process mode state (Additive/Subtractive) */
	int process_len;       /* Current window size */
	int process_offset;    /* Current window start index */
	bool process_building; /* true = adding notes, false = removing from start */

	int physical_keys_count;

	bool latch_enabled;
	enum arp_mode mode;
	bool arp_running;

	int timing_interval;
	int drift_cycle;

	struct k_mutex lock;
};

/**
 * @brief Result of an arpeggiator clock tick.
 */
struct arp_tick_result {
	bool base_triggered;
	bool high_triggered;
	uint8_t base_pitch;
	uint8_t high_pitch;
	uint8_t velocity;
};

/**
 * @brief Initialize the arpeggiator context.
 */
void arp_init(struct arp_ctx *ctx, int interval, int drift_cycle);

/**
 * @brief Handle a MIDI Note On event.
 */
void arp_note_on(struct arp_ctx *ctx, uint8_t pitch, uint8_t velocity);

/**
 * @brief Handle a MIDI Note Off event.
 */
void arp_note_off(struct arp_ctx *ctx, uint8_t pitch);

/**
 * @brief Set the latch mode state.
 */
void arp_set_latch(struct arp_ctx *ctx, bool enabled);

/**
 * @brief Set the arpeggiator mode.
 */
void arp_set_mode(struct arp_ctx *ctx, enum arp_mode mode);

/**
 * @brief Process a clock tick and determine if notes should trigger.
 */
struct arp_tick_result arp_tick(struct arp_ctx *ctx);

/**
 * @brief Get the currently sounding pitches to perform Note Off.
 */
void arp_get_current_pitches(struct arp_ctx *ctx, uint8_t *out_base, uint8_t *out_high);

/**
 * @brief Check if the arpeggiator is currently active.
 */
bool arp_is_running(struct arp_ctx *ctx);

/**
 * @brief Get the number of notes currently held in the buffer.
 */
int arp_get_num_notes(struct arp_ctx *ctx);

#endif /* ARP_H_ */
