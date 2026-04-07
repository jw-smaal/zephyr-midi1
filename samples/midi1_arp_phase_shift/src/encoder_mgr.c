/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file encoder_mgr.c
 * @brief Unified Input Manager with SHIFT and Pending Mode confirmation.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/logging/log.h>
#include "encoder_mgr.h"

LOG_MODULE_REGISTER(encoder_mgr, LOG_LEVEL_INF);

static struct arp_ctx *g_arp;
static struct tempo_ctx *g_tempo;
static enum encoder_param current_param = ENCODER_PARAM_DRIFT;
static enum arp_mode pending_mode = ARP_MODE_SYNC;
static bool shift_active = false;

static void input_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	if (evt->type == INPUT_EV_REL && evt->code == INPUT_REL_WHEEL) {
		int delta = evt->value;
		if (delta == 0) {
			return;
		}

		k_mutex_lock(&g_arp->lock, K_FOREVER);

		switch (current_param) {
		case ENCODER_PARAM_DRIFT: {
			int new_drift = g_arp->drift_cycle + delta;
			if (new_drift < 1) {
				new_drift = 1;
			}
			if (new_drift > 96) {
				new_drift = 96;
			}
			g_arp->drift_cycle = new_drift;
			LOG_INF("Encoder: Drift Cycle set to %d", g_arp->drift_cycle);
			break;
		}
		case ENCODER_PARAM_MODE: {
			/* Scroll through modes without applying yet */
			int new_m = (int)pending_mode + delta;
			if (new_m < 0) {
				new_m = (int)ARP_MODE_MAX - 1;
			}
			if (new_m >= (int)ARP_MODE_MAX) {
				new_m = 0;
			}
			pending_mode = (enum arp_mode)new_m;
			LOG_INF("Encoder: Mode selection: %d (Pending)", pending_mode);
			break;
		}
		case ENCODER_PARAM_BPM: {
			/* SHIFT now controlled by INPUT_KEY_3 */
			int32_t step = shift_active ? 1 : 100;
			int32_t new_bpm = g_tempo->current_bpm + (delta * step);
			tempo_set_bpm(g_tempo, new_bpm);
			break;
		}
		default:
			break;
		}

		k_mutex_unlock(&g_arp->lock);
	} else if (evt->type == INPUT_EV_KEY) {
		/* Track SHIFT state (D3 / INPUT_KEY_3) */
		if (evt->code == INPUT_KEY_3) {
			shift_active = evt->value;
			LOG_INF("SHIFT: %s", shift_active ? "ON" : "OFF");
		}

		/* Process other keys on 'Press' only */
		if (evt->value) {
			switch (evt->code) {
			case INPUT_KEY_0: /* SW2: Latch toggle only */
				arp_set_latch(g_arp, !g_arp->latch_enabled);
				LOG_INF("Latch Mode: %s", g_arp->latch_enabled ? "ON" : "OFF");
				break;
			case INPUT_KEY_1: /* SW3: Clear Arp */
				arp_clear(g_arp);
				LOG_INF("Arpeggiator Cleared");
				break;
			case INPUT_KEY_2: /* Encoder Button: Cycle focus & Apply Mode */
				if (current_param == ENCODER_PARAM_MODE) {
					arp_set_mode(g_arp, pending_mode);
					LOG_INF("Encoder: Mode Applied: %d", g_arp->mode);
				}

				current_param = (current_param + 1) % ENCODER_PARAM_MAX;

				if (current_param == ENCODER_PARAM_MODE) {
					pending_mode = g_arp->mode;
				}
				LOG_INF("Encoder Focus: %d", current_param);
				break;
			default:
				break;
			}
		}
	}
}

INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

int encoder_mgr_init(struct arp_ctx *arp, struct tempo_ctx *tempo)
{
	g_arp = arp;
	g_tempo = tempo;
	pending_mode = g_arp->mode;
	return 0;
}

enum encoder_param encoder_mgr_get_current_param(void)
{
	return current_param;
}

enum arp_mode encoder_mgr_get_pending_mode(void)
{
	return pending_mode;
}

bool encoder_mgr_is_shift_active(void)
{
	return shift_active;
}
