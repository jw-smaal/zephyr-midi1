/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief MIDI 1.0 Super Arpeggiator Sample
 *
 * Created in 2024 ported to Zephyr RTOS in 2024.
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260403
 *
 * This sample demonstrates an advanced arpeggiator that listens for MIDI Note On/Off
 * events over a serial MIDI interface and plays them back in sequence, synchronized
 * to a MIDI clock. It supports multiple directions (Up, Down, Up-Down, Down-Up) 
 * and octave spreading.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/logging/log.h>

/* In order to print out the note names (e.g., C4, D#5) */
#include "note.h"

LOG_MODULE_REGISTER(midi1_super_arp_sample, LOG_LEVEL_INF);

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

/* --- Arpeggiator Configuration --- */

#define ARP_OCTAVES 2  /* How many octaves to spread the arpeggio over */

enum arp_direction {
	ARP_DIR_UP,
	ARP_DIR_DOWN,
	ARP_DIR_UP_DOWN,
	ARP_DIR_DOWN_UP
};

/* --- Global State --- */

static struct note notes[MAX_NOTES];     /* Buffer of currently held notes */
static bool arp_running = false;         /* True if the arpeggiator is currently playing a note */
static uint8_t playing_pitch = 0;        /* The pitch of the currently sounding note (for note-off) */
static int num_active_notes = 0;         /* Number of notes currently in the 'notes' buffer */
static int current_note_idx = 0;         /* Index of the next note to play in the sequence */

static enum arp_direction current_dir = ARP_DIR_UP_DOWN; /* Current play direction */
static bool dir_going_up = true;                         /* Tracking for UP_DOWN/DOWN_UP ping-pong */
static int current_octave_offset = 0;                    /* Current octave multiplier */

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

static int clock_pulses = 0;             /* Counter for MIDI clock ticks (24 PPQN) */
static struct k_work arp_work;           /* Work item to offload arpeggio logic from ISR context */

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
		/* 1. Stop the previous note if one was playing */
		if (arp_running) {
			mid->note_off(midi_serial,
				      MY_MIDI1_CHAN,
				      playing_pitch,
				      0);
		}

		/* 2. Bounds check in case notes were removed while playing */
		if (current_note_idx >= num_active_notes) {
			current_note_idx = num_active_notes > 0 ? num_active_notes - 1 : 0;
		}
		if (current_note_idx < 0) {
			current_note_idx = 0;
		}

		/* 3. Calculate pitch with octave spread */
		int pitch = notes[current_note_idx].note + (current_octave_offset * 12);
		if (pitch > 127) {
			pitch = 127; /* Cap at max valid MIDI note */
		}
		playing_pitch = (uint8_t)pitch;

		/* 4. Start the next note using its original recorded velocity */
		mid->note_on(midi_serial,
			     MY_MIDI1_CHAN,
			     playing_pitch,
			     notes[current_note_idx].velocity);
		arp_running = true;

		/* 5. Advance the sequence based on direction */
		if (current_dir == ARP_DIR_UP) {
			current_note_idx++;
			if (current_note_idx >= num_active_notes) {
				current_note_idx = 0;
				current_octave_offset++;
			}
		} else if (current_dir == ARP_DIR_DOWN) {
			current_note_idx--;
			if (current_note_idx < 0) {
				current_note_idx = num_active_notes - 1;
				current_octave_offset++;
			}
		} else if (current_dir == ARP_DIR_UP_DOWN) {
			if (dir_going_up) {
				current_note_idx++;
				if (current_note_idx >= num_active_notes) {
					/* Reached the top, switch direction */
					current_note_idx = num_active_notes > 1 ? num_active_notes - 2 : 0;
					dir_going_up = false;
				}
			} else {
				current_note_idx--;
				if (current_note_idx < 0) {
					/* Reached the bottom, switch direction */
					current_note_idx = num_active_notes > 1 ? 1 : 0;
					dir_going_up = true;
					current_octave_offset++;
				}
			}
		} else if (current_dir == ARP_DIR_DOWN_UP) {
			if (!dir_going_up) {
				current_note_idx--;
				if (current_note_idx < 0) {
					/* Reached the bottom, switch direction */
					current_note_idx = num_active_notes > 1 ? 1 : 0;
					dir_going_up = true;
				}
			} else {
				current_note_idx++;
				if (current_note_idx >= num_active_notes) {
					/* Reached the top, switch direction */
					current_note_idx = num_active_notes > 1 ? num_active_notes - 2 : 0;
					dir_going_up = false;
					current_octave_offset++;
				}
			}
		}

		/* Wrap octaves */
		if (current_octave_offset >= ARP_OCTAVES) {
			current_octave_offset = 0;
		}
	} else {
		/* If no notes are held, ensure the arpeggiator is silent */
		if (arp_running) {
			mid->note_off(midi_serial,
				      MY_MIDI1_CHAN,
				      playing_pitch,
				      0);
			arp_running = false;
			
			/* Reset counters for next time notes are played */
			current_octave_offset = 0;
			if (current_dir == ARP_DIR_DOWN || current_dir == ARP_DIR_DOWN_UP) {
				/* Start from the top note when going down */
				current_note_idx = MAX_NOTES; /* Will bounds check on next play */
				dir_going_up = false;
			} else {
				current_note_idx = 0;
				dir_going_up = true;
			}
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
		 * We MUST offload MIDI transmission to a workqueue because
		 * sending data over UART/USB from an ISR is generally unsafe
		 * or blocking.
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
		 * MIDI Quirk: Many controllers send "Note On with Velocity 0"
		 * instead of a dedicated "Note Off" message. We handle both.
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
		/* Add new note to the end of our "chord" buffer if there's room */
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
K_THREAD_DEFINE(midi1_serial_receive_tid, 1024, midi1_serial_receive_thread, NULL, NULL, NULL, 5, 0,
		0);

int main(void)
{
	/* 1. Retrieve device pointers from Devicetree */
	midi_serial = DEVICE_DT_GET(DT_NODELABEL(midi1));
	midi_clock = DEVICE_DT_GET_ANY(midi1_clock_cntr);

	/* 2. Verify hardware readiness */
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

	/* 3. Signal the receive thread to start and init work item */
	k_sem_give(&init_sem);
	k_work_init(&arp_work, arp_work_handler);

	LOG_INF("Starting Super Arpeggiator Demo (Octaves: %d)", ARP_OCTAVES);

	/* 4. Register callback to be notified of clock ticks */
	clk->register_callback(midi_clock, clock_tick_callback);

	/* 5. Start the internal clock generation at the target BPM */
	clk->gen(midi_clock, TARGET_BPM);

	/* 6. Main loop for status logging */
	while (1) {
		k_mutex_lock(&arp_mutex, K_FOREVER);
		
		if (num_active_notes > 0) {
			const char *dir_str = "UP";
			if (current_dir == ARP_DIR_DOWN) {
				dir_str = "DOWN";
			} else if (current_dir == ARP_DIR_UP_DOWN) {
				dir_str = dir_going_up ? "UP_DOWN (Up)" : "UP_DOWN (Down)";
			} else if (current_dir == ARP_DIR_DOWN_UP) {
				dir_str = dir_going_up ? "DOWN_UP (Up)" : "DOWN_UP (Down)";
			}

			LOG_INF("Arp running | Octaves: +%d | Dir: %s | Notes: %d", 
				current_octave_offset, 
				dir_str,
				num_active_notes);
		} else {
			LOG_INF("Arpeggiator stopped.");
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
