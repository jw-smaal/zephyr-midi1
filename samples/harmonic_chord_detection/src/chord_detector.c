/*
 * SPDX-License-Identifier: Apache-2.0
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @file chord_detector.c
 * @date 20260504
 */

#include "chord_detector.h"
#include <zephyr/logging/log.h>
#include <zephyr/midi1/harmonic.h>
#include <string.h>

LOG_MODULE_REGISTER(chord_detector, LOG_LEVEL_INF);

#define MAX_SIMULTANEOUS_NOTES 16

static bool active_notes[128];
static harm_mask_t history_mask = 0;
static char last_chord_name[64] = "None";

static struct k_work_delayable reset_work;

static void reset_history_handler(struct k_work *work)
{
	history_mask = 0;
	LOG_INF("Note history reset due to silence.");
}

static void update_chord_detection(void)
{
	uint8_t notes_to_check[MAX_SIMULTANEOUS_NOTES];
	uint8_t count = 0;

	for (uint8_t i = 0; i < 128 && count < MAX_SIMULTANEOUS_NOTES; i++) {
		if (active_notes[i]) {
			notes_to_check[count++] = i;
			history_mask |= (1u << (i % 12));
		}
	}

	if (count == 0) {
		if (strcmp(last_chord_name, "None") != 0) {
			strcpy(last_chord_name, "None");
			LOG_INF("Detected: %s", last_chord_name);
		}
		/* Schedule reset after 3 seconds of silence */
		k_work_reschedule(&reset_work, K_MSEC(3000));
		return;
	}

	/* Cancel reset timer if notes are playing */
	k_work_cancel_delayable(&reset_work);

	char current_chord[64];
	harm_recognize_chord_from_notes(notes_to_check, count, current_chord, sizeof(current_chord));

	if (strcmp(current_chord, last_chord_name) != 0) {
		strcpy(last_chord_name, current_chord);
		LOG_INF("Detected Chord: %s", last_chord_name);

		/* Also perform scale detection on history */
		struct harm_scale_result results[3];
		int found = harm_recognize_scale(history_mask, results, 3);
		if (found > 0) {
			LOG_INF("Likely Scales:");
			for (int i = 0; i < found; i++) {
				LOG_INF("  - %s %s (%d%%)", 
				        harm_get_note_name(results[i].root, true),
				        results[i].name, results[i].score);
			}
		}
	}
}

static void note_on_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	if (velocity > 0) {
		active_notes[note] = true;
	} else {
		active_notes[note] = false;
	}
	update_chord_detection();
}

static void note_off_cb(uint8_t channel, uint8_t note, uint8_t velocity)
{
	active_notes[note] = false;
	update_chord_detection();
}

static const struct midi1_serial_callbacks detector_callbacks = {
	.note_on = note_on_cb,
	.note_off = note_off_cb,
};

int chord_detector_init(const struct device *dev)
{
	memset(active_notes, 0, sizeof(active_notes));
	k_work_init_delayable(&reset_work, reset_history_handler);
	return 0;
}

const struct midi1_serial_callbacks *chord_detector_get_callbacks(void)
{
	return &detector_callbacks;
}

const char *chord_detector_get_last_name(void)
{
	return last_chord_name;
}
