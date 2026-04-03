/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief MIDI 1.0 Arpeggiator Sample
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260403
 *
 * This sample demonstrates a simple arpeggiator that listens for MIDI Note On/Off
 * events over a serial MIDI interface and plays them back in sequence, synchronized
 * to a MIDI clock.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/logging/log.h>

/* In order to print out the note names (e.g., C4, D#5) */
#include "note.h"

LOG_MODULE_REGISTER(midi1_arp_sample, LOG_LEVEL_INF);

/* 
 * The target BPM for the internal clock generator.
 * Scaled by 100 (e.g., 12000 = 120.00 BPM).
 */
#define TARGET_BPM CONFIG_MIDI1_ARP_TARGET_BPM

/* 
 * MIDI channels are 0-indexed in the driver (0-15), 
 * while users typically think 1-16.
 */
#define MY_MIDI1_CHAN (CONFIG_MIDI1_SERIAL_CHANNEL - 1)

/* Maximum number of notes that can be held in the arpeggiator buffer */
#define MAX_NOTES CONFIG_MIDI1_ARP_NUMBER_OF_NOTES

struct note {
	uint8_t note;     /* MIDI Note Number (0-127) */
	uint8_t velocity; /* MIDI Velocity (0-127) */
};

/* --- Global State --- */

static struct note notes[MAX_NOTES];     /* Buffer of currently held notes */
static bool arp_running = false;         /* True if the arpeggiator is currently playing a note */
static uint8_t playing_pitch = 0;        /* The pitch of the currently sounding note (for note-off) */
static int num_active_notes = 0;         /* Number of notes currently in the 'notes' buffer */
static int current_note_idx = 0;         /* Index of the next note to play in the sequence */

/* 
 * Mutex to protect all shared state variables above.
 * This is critical because they are accessed by:
 * 1. The Receive Thread (note_on/off_handler)
 * 2. The System Work Queue (arp_work_handler)
 * 3. The Main Thread (logging)
 */
K_MUTEX_DEFINE(arp_mutex);

static const struct device *midi_serial;
static const struct midi1_serial_api *mid;
static const struct device *midi_clock;
static const struct midi1_clock_cntr_api *clk;

/* 
 * Semaphore used to ensure the receive thread doesn't start
 * until main() has finished initializing the device pointers.
 */
K_SEM_DEFINE(init_sem, 0, 1);

static int clock_pulses = 0;   /* Counter for MIDI clock ticks (24 PPQN) */
static struct k_work arp_work; /* Work item to offload arpeggio logic from ISR context */

/**
 * @brief Arpeggiator Work Handler
 *
 * This function executes in the system workqueue context. It handles the 
 * "Heavy lifting" of switching notes, which involves sending MIDI commands.
 */
void arp_work_handler(struct k_work *work)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);

	if (num_active_notes > 0) {
		/* Stop the previous note if one was playing */
		if (arp_running) {
			mid->note_off(midi_serial,
				      MY_MIDI1_CHAN,
				      playing_pitch,
				      0);
		}

		/* Bounds check/Wrap around the sequence index */
		if (current_note_idx >= num_active_notes) {
			current_note_idx = 0;
		}

		/* Start the next note using its original recorded velocity */
		playing_pitch = notes[current_note_idx].note;
		mid->note_on(midi_serial,
			     MY_MIDI1_CHAN,
			     playing_pitch,
			     notes[current_note_idx].velocity);
		arp_running = true;

		/* Advance the sequence */
		current_note_idx++;
	} else {
		/* If no notes are held, ensure the arpeggiator is silent */
		if (arp_running) {
			mid->note_off(midi_serial,
				      MY_MIDI1_CHAN,
				      playing_pitch,
				      0);
			arp_running = false;
		}
	}

	k_mutex_unlock(&arp_mutex);
}

/**
 * @brief MIDI Clock Tick Callback
 *
 * This is called from the MIDI Clock Counter driver's ISR context
 * every time a clock pulse occurs (usually 24 times per quarter note).
 */
void clock_tick_callback(void)
{
	clock_pulses++;

	/* 
	 * Trigger arpeggio step based on the configured timing interval.
	 * Default is usually 6 pulses for 16th notes (24 PPQN / 4 = 6).
	 */
	if (clock_pulses >= CONFIG_MIDI1_ARP_TIMING_INTERVAL) {
		clock_pulses = 0;
		/*
		 * offload MIDI transmission to a workqueue because
		 * sending data over UART/USB from an ISR is unsafe.
		 */
		k_work_submit(&arp_work);
	}
}

/**
 * @brief MIDI Note On Handler
 *
 * Called by the receiveparser whenever a Note On message is received.
 */
void note_on_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);

	if (velocity == 0) {
		/*
		 * Some controllers send a Note On with Velocity 0 which is
		 * the same as Note Off.  Especially when using running status.
		 */
		for (int i = 0; i < num_active_notes; i++) {
			if (notes[i].note == note) {
				/* Remove the note and shift the remaining ones down */
				for (int j = i; j < num_active_notes - 1; j++) {
					notes[j] = notes[j + 1];
				}
				num_active_notes--;
				break;
			}
		}
	} else {
		/* Add new note to the end of our buffer if there's room */
		if (num_active_notes < MAX_NOTES) {
			notes[num_active_notes].note = note;
			notes[num_active_notes].velocity = velocity;
			num_active_notes++;
		}
	}
	k_mutex_unlock(&arp_mutex);
}

/**
 * @brief MIDI Note Off Handler
 *
 * Called by the receiveparser whenever a Note Off message is received.
 */
void note_off_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);

	/* Find the note in our buffer and remove it */
	for (int i = 0; i < num_active_notes; i++) {
		if (notes[i].note == note) {
			for (int j = i; j < num_active_notes - 1; j++) {
				notes[j] = notes[j + 1];
			}
			num_active_notes--;
			break;
		}
	}
	k_mutex_unlock(&arp_mutex);
}

/**
 * @brief MIDI Receive Thread
 *
 * Dedicated thread for processing incoming serial MIDI data.
 */
void midi1_serial_receive_thread(void)
{
	/* Wait until main() says hardware is ready */
	k_sem_take(&init_sem, K_FOREVER);

	if (!device_is_ready(midi_serial)) {
		LOG_ERR("receive_thread Serial MIDI1 device not ready");
		return;
	}

	/* Register our app callbacks with the driver-level parser */
	struct midi1_serial_callbacks my_cb = {
		.note_on = note_on_handler,
		.note_off = note_off_handler,
	};
	mid->register_callbacks(midi_serial, &my_cb);

	while (1) {
		/* 
		 * Continuously poll the UART and run the state machine 
		 * to parse MIDI bytes into messages.
		 */
		mid->receiveparser(midi_serial);
	}
}

/* Define the receive thread with priority 5 */
K_THREAD_DEFINE(midi1_serial_receive_tid, 1024,
		midi1_serial_receive_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	/* Retrieve device pointers from Devicetree */
	midi_serial = DEVICE_DT_GET(DT_NODELABEL(midi1));
	midi_clock = DEVICE_DT_GET_ANY(midi1_clock_cntr);

	/* Verify hardware readiness */
	if (!device_is_ready(midi_serial)) {
		LOG_ERR("Serial MIDI device midi1 not ready");
		return -ENODEV;
	}
	mid = midi_serial->api;

	if (midi_clock == NULL || !device_is_ready(midi_clock)) {
		LOG_ERR("MIDI clock counter device not ready");
		return -ENODEV;
	}
	clk = midi_clock->api;

	/* Signal the receive thread to start and init work item */
	k_sem_give(&init_sem);
	k_work_init(&arp_work, arp_work_handler);

	LOG_INF("Starting Arpeggiator Demo");

	/* Register callback to be notified of clock ticks */
	clk->register_callback(midi_clock, clock_tick_callback);

	/* Start the internal clock generation at the target BPM */
	clk->gen(midi_clock, TARGET_BPM);

	/* Main loop for status logging */
	while (1) {
		k_mutex_lock(&arp_mutex, K_FOREVER);
		
		if (num_active_notes > 0) {
			LOG_INF("Arpeggiator running. Notes held: %d", num_active_notes);
		} else {
			LOG_INF("Arpeggiator stopped. Waiting for notes...");
		}

		/* Print out the current buffer of notes with their velocities */
		for (uint8_t i = 0; i < num_active_notes; i++) {
			LOG_INF("[%d]: %s (Vel: %d)", i,
				noteToTextWithOctave(notes[i].note, false),
				notes[i].velocity);
		}
		
		k_mutex_unlock(&arp_mutex);
		
		k_sleep(K_SECONDS(2));
	}

	return 0;
}
