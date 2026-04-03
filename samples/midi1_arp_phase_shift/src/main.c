/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief MIDI 1.0 Phasing Arpeggiator
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @updated 20260403
 *
 * This sample implements "Phasing" by running two layers at slightly
 * different clock intervals. The base layer is steady, while the upper 
 * octave layer "drifts" very slowly, creating a shifting rhythmic pattern.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "note.h"

LOG_MODULE_REGISTER(midi1_arp_phase_sample, LOG_LEVEL_INF);

#define TARGET_BPM CONFIG_MIDI1_ARP_TARGET_BPM
#define MY_MIDI1_CHAN (CONFIG_MIDI1_SERIAL_CHANNEL - 1)
#define MAX_NOTES CONFIG_MIDI1_ARP_NUMBER_OF_NOTES

/* Timing configuration */
#define BASE_INTERVAL CONFIG_MIDI1_ARP_TIMING_INTERVAL

/* 
 * The Phasing Drift: 
 * Every 'DRIFT_CYCLE' notes played, the high octave will wait 
 * 1 extra pulse. This causes the layers to gradually drift apart.
 */
#define DRIFT_CYCLE 8

/* LEDs/Button */
static const struct gpio_dt_spec latch_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);  /* Red */
static const struct gpio_dt_spec phase_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);  /* Green */
static const struct gpio_dt_spec tempo_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios);  /* Blue */
static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);      /* SW2 */
static const struct gpio_dt_spec phase_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios); /* SW3 */
static struct gpio_callback button_cb_data;
static struct gpio_callback phase_button_cb_data;

struct note {
	uint8_t note;
	uint8_t velocity;
};

/* --- Global State --- */

static struct note notes[MAX_NOTES];
static bool arp_running = false;
static int num_active_notes = 0;

/* Base Layer state */
static uint8_t base_pitch = 0;
static int base_idx = 0;
static int base_clock = 0;

/* High Layer state (The "Phasing" layer) */
static uint8_t high_pitch = 0;
static int high_idx = 0;
static int high_clock = 0;
static int high_trigger_count = 0;
static bool high_lag_pending = false;

static bool latch_enabled = false;
static bool phasing_enabled = false; /* Starts perfectly in sync */
static int physical_keys_count = 0;

K_MUTEX_DEFINE(arp_mutex);

static const struct device *midi_serial;
static const struct midi1_serial_api *mid;
static const struct device *midi_clock_dev;
static const struct midi1_clock_cntr_api *clk;

K_SEM_DEFINE(init_sem, 0, 1);

static int beat_pulses = 0;
static struct k_work base_on_work;
static struct k_work high_on_work;
static struct k_work tempo_work;

void tempo_work_handler(struct k_work *work)
{
	static bool led_state = false;
	led_state = !led_state;
	gpio_pin_set_dt(&tempo_led, led_state);
}

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);
	latch_enabled = !latch_enabled;
	/* 
	 * Red LED (latch_led) is active-low on this board.
	 * Set to HIGH (1) to turn OFF when latch is disabled.
	 */
	gpio_pin_set_dt(&latch_led, !latch_enabled);
	if (!latch_enabled && physical_keys_count == 0) {
		num_active_notes = 0;
	}
	k_mutex_unlock(&arp_mutex);
}

void phase_button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);
	phasing_enabled = !phasing_enabled;
	gpio_pin_set_dt(&phase_led, phasing_enabled);
	
	if (!phasing_enabled) {
		/* Snap back into phase */
		high_trigger_count = 0;
		high_lag_pending = false;
		high_clock = base_clock;
		if (num_active_notes > 0) {
			high_idx = num_active_notes - 1 - base_idx;
			if (high_idx < 0) {
				high_idx = 0;
			}
		}
	}
	
	k_mutex_unlock(&arp_mutex);
}

void base_on_handler(struct k_work *work)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);
	if (num_active_notes > 0) {
		mid->note_off(midi_serial, MY_MIDI1_CHAN, base_pitch, 0);
		if (++base_idx >= num_active_notes) base_idx = 0;
		base_pitch = notes[base_idx].note;
		mid->note_on(midi_serial, MY_MIDI1_CHAN, base_pitch, notes[base_idx].velocity);
		arp_running = true;
	} else if (arp_running) {
		mid->note_off(midi_serial, MY_MIDI1_CHAN, base_pitch, 0);
	}
	k_mutex_unlock(&arp_mutex);
}

void high_on_handler(struct k_work *work)
{
	k_mutex_lock(&arp_mutex, K_FOREVER);
	if (num_active_notes > 0) {
		mid->note_off(midi_serial, MY_MIDI1_CHAN, high_pitch, 0);
		if (--high_idx < 0) high_idx = num_active_notes - 1;
		int pitch_calc = notes[high_idx].note + 12;
		high_pitch = (pitch_calc <= 127) ? (uint8_t)pitch_calc : 127;
		mid->note_on(midi_serial, MY_MIDI1_CHAN, high_pitch, notes[high_idx].velocity);
	} else if (arp_running) {
		mid->note_off(midi_serial, MY_MIDI1_CHAN, high_pitch, 0);
	}
	k_mutex_unlock(&arp_mutex);
}

/**
 * @brief MIDI Clock Tick Callback
 */
void clock_tick_callback(void)
{
	base_clock++;
	high_clock++;
	beat_pulses++;

	/* 1. Base Layer (Steady) */
	if (base_clock >= BASE_INTERVAL) {
		base_clock = 0;
		k_work_submit(&base_on_work);
	}

	/* 2. High Layer (Drifting if enabled) */
	int current_high_interval = BASE_INTERVAL;
	if (phasing_enabled && high_lag_pending) {
		/* Apply the 1-pulse lag */
		current_high_interval += 1;
	}

	if (high_clock >= current_high_interval) {
		high_clock = 0;
		
		if (phasing_enabled) {
			high_lag_pending = false; /* Reset after the lag is applied */
		}
		
		k_work_submit(&high_on_work);

		/* Schedule next drift */
		if (phasing_enabled) {
			high_trigger_count++;
			if (high_trigger_count >= DRIFT_CYCLE) {
				high_trigger_count = 0;
				high_lag_pending = true;
			}
		}
	}

	/* 3. Tempo LED */
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
	if (latch_enabled && physical_keys_count == 0) {
		num_active_notes = 0;
		/* Reset phasing state on new chord */
		high_trigger_count = 0;
		high_lag_pending = false;
		high_clock = 0;
		base_clock = 0;
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
	if (physical_keys_count > 0) physical_keys_count--;
	for (int i = 0; i < num_active_notes; i++) {
		if (notes[i].note == note) {
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
	struct midi1_serial_callbacks my_cb = {.note_on = note_on_handler, .note_off = note_off_handler};
	mid->register_callbacks(midi_serial, &my_cb);
	while (1) mid->receiveparser(midi_serial);
}

K_THREAD_DEFINE(midi1_serial_receive_tid, 1024, midi1_serial_receive_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	if (!gpio_is_ready_dt(&button) || !gpio_is_ready_dt(&latch_led) || !gpio_is_ready_dt(&tempo_led) ||
	    !gpio_is_ready_dt(&phase_button) || !gpio_is_ready_dt(&phase_led)) return -ENODEV;
	
	gpio_pin_configure_dt(&button, GPIO_INPUT);
	gpio_pin_configure_dt(&phase_button, GPIO_INPUT);
	
	gpio_pin_configure_dt(&latch_led, GPIO_OUTPUT_ACTIVE); /* High = OFF */
	gpio_pin_configure_dt(&tempo_led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&phase_led, GPIO_OUTPUT_INACTIVE);
	
	gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
	gpio_add_callback(button.port, &button_cb_data);

	gpio_pin_interrupt_configure_dt(&phase_button, GPIO_INT_EDGE_TO_ACTIVE);
	gpio_init_callback(&phase_button_cb_data, phase_button_pressed, BIT(phase_button.pin));
	gpio_add_callback(phase_button.port, &phase_button_cb_data);

	midi_serial = DEVICE_DT_GET(DT_NODELABEL(midi1));
	midi_clock_dev = DEVICE_DT_GET_ANY(midi1_clock_cntr);
	if (!device_is_ready(midi_serial)) return -ENODEV;
	mid = midi_serial->api;
	if (midi_clock_dev == NULL || !device_is_ready(midi_clock_dev)) return -ENODEV;
	clk = midi_clock_dev->api;

	k_sem_give(&init_sem);
	k_work_init(&base_on_work, base_on_handler);
	k_work_init(&high_on_work, high_on_handler);
	k_work_init(&tempo_work, tempo_work_handler);

	LOG_INF("Phasing Arpeggiator Started.");

	clk->register_callback(midi_clock_dev, clock_tick_callback);
	clk->gen(midi_clock_dev, TARGET_BPM);

	while (1) k_sleep(K_SECONDS(2));
	return 0;
}
