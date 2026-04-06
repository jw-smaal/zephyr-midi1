/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file main.c
 * @brief MIDI 1.0 Phasing Arpeggiator Application
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_cntr.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include "note.h"
#include "arp.h"
#include "tempo.h"
#include "display_mgr.h"
#include "encoder_mgr.h"

LOG_MODULE_REGISTER(midi1_arp_phase_app, LOG_LEVEL_INF);

#define TARGET_BPM    CONFIG_MIDI1_ARP_TARGET_BPM
#define MY_MIDI1_CHAN (CONFIG_MIDI1_SERIAL_CHANNEL - 1)
/* #define DRIFT_CYCLE 8 */
#define DRIFT_CYCLE   CONFIG_MIDI1_ARP_DRIFT_CYCLE

/* MIDI Clock standard is 24 Pulses Per Quarter Note (PPQN) */
#define MIDI_CLOCK_PPQN         24
/* Toggle the tempo LED every half-beat for a 50% duty cycle */
#define TEMPO_LED_TOGGLE_PULSES (MIDI_CLOCK_PPQN / 2)

/* Hardware Specifications */
static const struct gpio_dt_spec latch_led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios); /* Red */
static const struct gpio_dt_spec mode_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);  /* Green */
static const struct gpio_dt_spec tempo_led = GPIO_DT_SPEC_GET(DT_ALIAS(led2), gpios); /* Blue */

/* MIDI Devices */
static const struct device *midi_serial;
static const struct midi1_serial_api *mid;
static const struct device *midi_clock_dev;
static const struct midi1_clock_cntr_api *clk;

/* State & Synchronisation */
static struct arp_ctx arp;
static struct tempo_ctx tempo;
K_SEM_DEFINE(init_sem, 0, 1);

static struct k_work arp_step_work;
static struct k_work tempo_work;

/**
 * @brief Work handler for the tempo LED flash.
 */
void tempo_work_handler(struct k_work *work)
{
	static bool led_state = false;
	led_state = !led_state;
	gpio_pin_set_dt(&tempo_led, led_state);
}

/**
 * @brief Input Subsystem callback for UI LED feedback.
 */
static void ui_input_cb(struct input_event *evt, void *user_data)
{
	if (evt->type == INPUT_EV_KEY && evt->value) {
		switch (evt->code) {
		case INPUT_KEY_0: /* Latch */
			/* Update Red LED: ON when latch is active (active-low) */
			gpio_pin_set_dt(&latch_led, !arp.latch_enabled);
			break;
		case INPUT_KEY_1: /* Clear */
			/* Flash Green LED briefly to confirm clear */
			gpio_pin_set_dt(&mode_led, 1);
			k_sleep(K_MSEC(100));
			gpio_pin_set_dt(&mode_led, 0);
			break;
		default:
			break;
		}
	}
}

INPUT_CALLBACK_DEFINE(NULL, ui_input_cb, NULL);

/**
 * @brief Executes the MIDI note changes for an arpeggio step.
 */
void arp_step_handler(struct k_work *work)
{
	static bool was_running = false;
	struct arp_tick_result result;
	uint8_t current_base, current_high;
	bool is_running;

	/* Get the pitches that are currently sounding to turn them off */
	arp_get_current_pitches(&arp, &current_base, &current_high);

	/* Determine if we need to trigger new notes */
	result = arp_tick(&arp);

	if (result.base_triggered) {
		mid->note_off(midi_serial, MY_MIDI1_CHAN, current_base, 0);
		mid->note_on(midi_serial, MY_MIDI1_CHAN, result.base_pitch, result.velocity);
	}

	if (result.high_triggered) {
		mid->note_off(midi_serial, MY_MIDI1_CHAN, current_high, 0);
		mid->note_on(midi_serial, MY_MIDI1_CHAN, result.high_pitch, result.velocity);
	}

	is_running = arp_is_running(&arp);

	/* Global silence check (only trigger once when stopping) */
	if (!is_running && was_running) {
		if (current_base > 0) {
			mid->note_off(midi_serial, MY_MIDI1_CHAN, current_base, 0);
		}
		if (current_high > 0) {
			mid->note_off(midi_serial, MY_MIDI1_CHAN, current_high, 0);
		}
	}

	was_running = is_running;
}

/**
 * @brief MIDI Clock Callback (ISR Context).
 */
void clock_tick_callback(void)
{
	static int beat_pulses = 0;

	k_work_submit(&arp_step_work);

	if (++beat_pulses >= TEMPO_LED_TOGGLE_PULSES) {
		beat_pulses = 0;
		k_work_submit(&tempo_work);
	}
}

/**
 * @brief MIDI Note On Handler.
 */
void note_on_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	arp_note_on(&arp, note, velocity);
}

/**
 * @brief MIDI Note Off Handler.
 */
void note_off_handler(uint8_t channel, uint8_t note, uint8_t velocity)
{
	arp_note_off(&arp, note);
}

/**
 * @brief MIDI Control Change Handler
 */
void control_change_handler(uint8_t channel, uint8_t controller, uint8_t value)
{
	switch (controller) {
	case CTL_MSB_MODWHEEL:
		break;
	case CTL_SUSTAIN:
		if (value == 0) {
			/* We want to use the Sus pedal as a toggle switch */
			break;
		}
		arp_set_latch(&arp, !arp.latch_enabled);
		/* Red LED is active-low: 0 = ON, 1 = OFF */
		gpio_pin_set_dt(&latch_led, !arp.latch_enabled);
		LOG_INF("Sustain Pedal Latch: %s", arp.latch_enabled ? "ON" : "OFF");
		break;
	default:
		break;
	}
}

/**
 * @brief Background thread for MIDI parsing.
 */
void midi_rx_thread(void)
{
	k_sem_take(&init_sem, K_FOREVER);

	struct midi1_serial_callbacks cb = {
		.note_on = note_on_handler,
		.note_off = note_off_handler,
		.control_change = control_change_handler,
	};
	mid->register_callbacks(midi_serial, &cb);

	while (1) {
		mid->receiveparser(midi_serial);
	}
}

K_THREAD_DEFINE(midi_rx_tid, 512, midi_rx_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
	int err;

	/* 1. Hardware Setup */
	if (!gpio_is_ready_dt(&latch_led) || !gpio_is_ready_dt(&tempo_led) ||
	    !gpio_is_ready_dt(&mode_led)) {
		return -ENODEV;
	}

	gpio_pin_configure_dt(&latch_led, GPIO_OUTPUT_ACTIVE); /* High = OFF */
	gpio_pin_configure_dt(&tempo_led, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&mode_led, GPIO_OUTPUT_INACTIVE);

	midi_serial = DEVICE_DT_GET(DT_NODELABEL(midi1));
	midi_clock_dev = DEVICE_DT_GET_ANY(midi1_clock_cntr);
	if (!device_is_ready(midi_serial) || !device_is_ready(midi_clock_dev)) {
		return -ENODEV;
	}
	mid = midi_serial->api;
	clk = midi_clock_dev->api;

	/* 2. Logic Initialization */
	arp_init(&arp, CONFIG_MIDI1_ARP_TIMING_INTERVAL, DRIFT_CYCLE);

	/* Initialize Encoder */
	err = encoder_mgr_init(&arp, &tempo);
	if (err < 0) {
		LOG_ERR("Failed to initialize encoder manager: %d", err);
		/* Non-fatal error */
	}

	/* Initialize Tempo module (Digital) */
	err = tempo_init(&tempo, midi_clock_dev);
	if (err < 0) {
		return err;
	}

	k_work_init(&arp_step_work, arp_step_handler);
	k_work_init(&tempo_work, tempo_work_handler);

	/* Display Setup (Modular) */
	display_mgr_init(&arp, &tempo);

	k_sem_give(&init_sem);

	LOG_INF("Phasing Arpeggiator (Modular) Started.");

	clk->register_callback(midi_clock_dev, clock_tick_callback);
	/* Initial clock start */
	clk->gen(midi_clock_dev, TARGET_BPM);

	while (1) {
		k_mutex_lock(&arp.lock, K_FOREVER);

		if (arp.num_notes > 0) {
			char chord_buf[128] = {0};
			char base_str[8], high_str[8], now_base[8], now_high[8];
			int pos = 0;

			snprintf(now_base, sizeof(now_base), "%s",
				 noteToTextWithOctave(arp.base.playing_pitch, false));
			snprintf(now_high, sizeof(now_high), "%s",
				 noteToTextWithOctave(arp.high.playing_pitch, false));

			for (int i = 0; i < arp.num_notes; i++) {
				uint8_t h = (arp.notes[i].pitch + 12 <= 127)
						    ? arp.notes[i].pitch + 12
						    : 127;

				snprintf(base_str, sizeof(base_str), "%s",
					 noteToTextWithOctave(arp.notes[i].pitch, false));
				snprintf(high_str, sizeof(high_str), "%s",
					 noteToTextWithOctave(h, false));

				pos += snprintf(chord_buf + pos, sizeof(chord_buf) - pos,
						"[%s->%s] ", base_str, high_str);

				if (pos >= sizeof(chord_buf) - 1) {
					break;
				}
			}

			const char *m_str = "SYNC";
			if (arp.mode == ARP_MODE_PHASE) {
				m_str = "PHASE";
			} else if (arp.mode == ARP_MODE_PROCESS) {
				m_str = "PROCESS";
			} else if (arp.mode == ARP_MODE_PHASE_PROCESS) {
				m_str = "PH+PROC";
			}

			LOG_INF("ARP [%s] MODE [%s] | %d:%d | Now: %s/%s",
				arp.latch_enabled ? "ON" : "OFF", m_str, arp.process_offset,
				arp.process_len, now_base, now_high);
			LOG_INF("Chord: %s", chord_buf);
		}

		k_mutex_unlock(&arp.lock);
		k_sleep(K_SECONDS(2));
	}

	return 0;
}
