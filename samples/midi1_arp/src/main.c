/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(midi1_arp_sample, LOG_LEVEL_INF);

/* 120.00 BPM scaled by 100 */
#define TARGET_BPM 12000

#define MY_MIDI1_CHAN (CONFIG_MIDI1_SERIAL_CHANNEL - 1)

#define MAX_NOTES 10
static uint8_t active_notes[MAX_NOTES];
static int num_active_notes = 0;
static int current_note_idx = 0;

static const struct device *midi_serial;
static const struct midi1_serial_api *mid;
static const struct device *midi_clock;
static const struct midi1_clock_cntr_api *clk;

static int clock_pulses = 0;
static uint8_t playing_note = 255;
static struct k_work arp_work;

void arp_work_handler(struct k_work *work)
{
	/* Simple arpeggiator logic on 16th notes */
	if (num_active_notes > 0) {
		if (playing_note != 255) {
			mid->note_off(midi_serial, MY_MIDI1_CHAN, playing_note, 0);
		}

		if (current_note_idx >= num_active_notes) {
			current_note_idx = 0;
		}

		playing_note = active_notes[current_note_idx];
		mid->note_on(midi_serial, MY_MIDI1_CHAN, playing_note, 100);
		current_note_idx++;
	} else {
		if (playing_note != 255) {
			mid->note_off(midi_serial, MY_MIDI1_CHAN, playing_note, 0);
			playing_note = 255;
		}
	}
}

void clock_tick_callback(void)
{
	clock_pulses++;
	/* Trigger every 6th pulse (16th notes: 24 PPQN / 4 = 6) */
	if (clock_pulses >= 6) {
		clock_pulses = 0;
		/* Submit work item to avoid blocking in callback context */
		k_work_submit(&arp_work);
	}
}

void note_on_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	if (velocity == 0) {
		/* Some controllers send Note On with 0 velocity for Note Off */
		for (int i = 0; i < num_active_notes; i++) {
			if (active_notes[i] == note) {
				for (int j = i; j < num_active_notes - 1; j++) {
					active_notes[j] = active_notes[j+1];
				}
				num_active_notes--;
				break;
			}
		}
	} else {
		if (num_active_notes < MAX_NOTES) {
			active_notes[num_active_notes++] = note;
		}
	}
}

void note_off_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	for (int i = 0; i < num_active_notes; i++) {
		if (active_notes[i] == note) {
			for (int j = i; j < num_active_notes - 1; j++) {
				active_notes[j] = active_notes[j+1];
			}
			num_active_notes--;
			break;
		}
	}
}

void midi1_serial_receive_thread(void)
{
	if (!device_is_ready(midi_serial)) {
		LOG_ERR("receive_thread Serial MIDI1 device not ready");
		return;
	}

	struct midi1_serial_callbacks my_cb = {
		.note_on = note_on_handler,
		.note_off = note_off_handler,
	};
	mid->register_callbacks(midi_serial, &my_cb);

	while (1) {
		mid->receiveparser(midi_serial);
	}
}

K_THREAD_DEFINE(midi1_serial_receive_tid, 1024,
                midi1_serial_receive_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	midi_serial = DEVICE_DT_GET(DT_NODELABEL(midi0));
	midi_clock = DEVICE_DT_GET_ANY(midi1_clock_cntr);

	if (!device_is_ready(midi_serial)) {
		LOG_ERR("Serial MIDI device midi0 not ready");
		return -ENODEV;
	}
	mid = midi_serial->api;

	if (midi_clock == NULL || !device_is_ready(midi_clock)) {
		LOG_ERR("MIDI clock counter device not ready");
		return -ENODEV;
	}
	clk = midi_clock->api;

	k_work_init(&arp_work, arp_work_handler);

	LOG_INF("Starting Arpeggiator Demo");

	/* Register the clock callback for the internal clock generation */
	clk->register_callback(midi_clock, clock_tick_callback);

	/* Start the clock generation */
	clk->gen(midi_clock, TARGET_BPM);

	while (1) {
		LOG_INF("Arpeggiator running");
		k_sleep(K_SECONDS(5));
	}

	return 0;
}
