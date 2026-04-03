/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief MIDI 1.0 Converging Arpeggiator with Latch and Tempo Heartbeat
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260403
 *
 * Plays counter-point arpeggios. 
 * - SW2: Toggles Latch (Red LED on when Latch active)
 * - Blue LED: Flashes at the current tempo (every quarter note).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "note.h"

LOG_MODULE_REGISTER(midi1_octave_arp_sample, LOG_LEVEL_INF);

#define TARGET_BPM CONFIG_MIDI1_ARP_TARGET_BPM
#define MY_MIDI1_CHAN (CONFIG_MIDI1_SERIAL_CHANNEL - 1)
#define MAX_NOTES CONFIG_MIDI1_ARP_NUMBER_OF_NOTES

/* LED/Button configuration */
static const struct gpio_dt_spec latch_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);  /* Red */
static const struct gpio_dt_spec tempo_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);  /* Blue */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);     /* SW2 */
static struct gpio_callback button_cb_data;

struct note {
	uint8_t note;
	uint8_t velocity;
};

/* --- Global State --- */

static struct note notes[MAX_NOTES];
static bool arp_running = false;
static uint8_t playing_pitch = 0;
static uint8_t playing_high_pitch = 0;
static int num_active_notes = 0;
static int current_note_idx = 0;

/* Latch logic */
static bool latch_enabled = false;
static int physical_keys_count = 0; /* Number of keys physically held down */

K_MUTEX_DEFINE(arp_mutex);

static const struct device *midi_serial;
static const struct midi1_serial_api *mid;
static const struct device *midi_clock;
static const struct midi1_clock_cntr_api *clk;

K_SEM_DEFINE(init_sem, 0, 1);

static int clock_pulses = 0;
static int beat_pulses = 0;
static struct k_work arp_work;
static struct k_work tempo_work;

/**
 * @brief Toggle Tempo LED (Blue)
 */
void tempo_work_handler(struct k_work *work)
{
	static bool led_state = false;
	led_state = !led_state;
	gpio_pin_set_dt(&tempo_led, led_state);
}

/**
 * @brief Button ISR - Toggle Latch
 */
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);
	latch_enabled = !latch_enabled;
	gpio_pin_set_dt(&latch_led, latch_enabled);
	
	/* 
	 * Fix: If Latch is turned OFF and no physical keys are held,
	 * clear the arpeggio immediately.
	 */
	if (!latch_enabled && physical_keys_count == 0) {
		num_active_notes = 0;
	}

	LOG_INF("Latch Mode: %s", latch_enabled ? "ON" : "OFF");
	k_mutex_unlock(&arp_mutex);
}

void arp_work_handler(struct k_work *work)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);

	if (num_active_notes > 0) {
		if (arp_running) {
			mid->note_off(midi_serial, MY_MIDI1_CHAN, playing_pitch, 0);
			mid->note_off(midi_serial, MY_MIDI1_CHAN, playing_high_pitch, 0);
		}

		if (current_note_idx >= num_active_notes) {
			current_note_idx = 0;
		}

		playing_pitch = notes[current_note_idx].note;
		int high_idx = num_active_notes - 1 - current_note_idx;
		int high_pitch_calc = notes[high_idx].note + 12;
		playing_high_pitch = (high_pitch_calc <= 127) ? (uint8_t)high_pitch_calc : 127;

		uint8_t base_vel = notes[current_note_idx].velocity;
		uint8_t high_vel = notes[high_idx].velocity;

		mid->note_on(midi_serial, MY_MIDI1_CHAN, playing_pitch, base_vel);
		mid->note_on(midi_serial, MY_MIDI1_CHAN, playing_high_pitch, high_vel);
		arp_running = true;
		current_note_idx++;
	} else {
		if (arp_running) {
			mid->note_off(midi_serial, MY_MIDI1_CHAN, playing_pitch, 0);
			mid->note_off(midi_serial, MY_MIDI1_CHAN, playing_high_pitch, 0);
			arp_running = false;
			current_note_idx = 0;
		}
	}

	k_mutex_unlock(&arp_mutex);
}

/**
 * @brief MIDI Clock Tick Callback
 */
void clock_tick_callback(void)
{
	clock_pulses++;
	beat_pulses++;

	if (clock_pulses >= CONFIG_MIDI1_ARP_TIMING_INTERVAL) {
		clock_pulses = 0;
		k_work_submit(&arp_work);
	}

	if (beat_pulses >= 12) {
		beat_pulses = 0;
		k_work_submit(&tempo_work);
	}
}

void note_on_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);
	
	if (velocity == 0) {
		goto note_off_logic;
	}

	/* If this is the start of a new chord, clear the old latched notes */
	if (latch_enabled && physical_keys_count == 0) {
		num_active_notes = 0;
	}
	physical_keys_count++;

	if (num_active_notes < MAX_NOTES) {
		notes[num_active_notes].note = note;
		notes[num_active_notes].velocity = velocity;
		num_active_notes++;
	}
	k_mutex_unlock(&arp_mutex);
	return;

note_off_logic:
	if (physical_keys_count > 0) {
		physical_keys_count--;
	}

	for (int i = 0; i < num_active_notes; i++) {
		if (notes[i].note == note) {
			/* If latch is OFF, we remove the note from the sequence immediately */
			if (!latch_enabled) {
				for (int j = i; j < num_active_notes - 1; j++) {
					notes[j] = notes[j + 1];
				}
				num_active_notes--;
			}
			break;
		}
	}
	k_mutex_unlock(&arp_mutex);
}

void note_off_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	note_on_handler(channel, note, 0);
}

void midi1_serial_receive_thread(void)
{
	k_sem_take(&init_sem, K_FOREVER);
	if (!device_is_ready(midi_serial)) return;

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
	/* 1. Init GPIO */
	if (!gpio_is_ready_dt(&button) || !gpio_is_ready_dt(&latch_led) || !gpio_is_ready_dt(&tempo_led)) {
		return -ENODEV;
	}
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_configure_dt(&latch_led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&tempo_led, GPIO_OUTPUT_INACTIVE);

	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	/* 2. Init MIDI */
	midi_serial = DEVICE_DT_GET(DT_NODELABEL(midi1));
	midi_clock = DEVICE_DT_GET_ANY(midi1_clock_cntr);
	if (!device_is_ready(midi_serial)) return -ENODEV;
	mid = midi_serial->api;
	if (midi_clock == NULL || !device_is_ready(midi_clock)) return -ENODEV;
	clk = midi_clock->api;

	k_sem_give(&init_sem);
	k_work_init(&arp_work, arp_work_handler);
	k_work_init(&tempo_work, tempo_work_handler);

	LOG_INF("Converging Arpeggiator (Latch: Red, Tempo: Blue)");

	clk->register_callback(midi_clock, clock_tick_callback);
	clk->gen(midi_clock, TARGET_BPM);

	while (1) {
		k_sleep(K_SECONDS(2));
	}

	return 0;
}
