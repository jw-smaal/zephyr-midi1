/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file display_mgr.c
 * @brief Modular Display Manager implementation.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

#include "display_mgr.h"
#include "note.h"
#include "encoder_mgr.h"

LOG_MODULE_REGISTER(display_mgr, LOG_LEVEL_WRN);

/* Stack for the background display thread */
K_THREAD_STACK_DEFINE(display_stack, 1024);
static struct k_thread display_thread_data;

static struct arp_ctx *g_arp;
static struct tempo_ctx *g_tempo;
static const struct device *display_dev;

/**
 * @brief Background thread for display updates.
 */
void display_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	static struct {
		enum arp_mode mode;
		bool latch;
		int32_t bpm;
		uint8_t base_pitch;
		uint8_t high_pitch;
		int num_notes;
		int poly_offset;
		int drift_cycle;
		enum encoder_param param;
		bool shifted;
		enum arp_mode pending_mode;
	} last_state = {.mode = 255};

	char line1[32], line2[32], line3[32], line4[32];
	uint8_t font_height;

	if (display_dev == NULL || !device_is_ready(display_dev)) {
		return;
	}

	cfb_get_font_size(display_dev, 0, NULL, &font_height);

	while (1) {
		bool changed = false;
		enum encoder_param current_p = encoder_mgr_get_current_param();
		bool current_s = encoder_mgr_is_shift_active();

		enum arp_mode current_m = encoder_mgr_get_pending_mode();

		/* --- Step 1: Rapid Snapshot --- */
		k_mutex_lock(&g_arp->lock, K_FOREVER);

		if (g_arp->mode != last_state.mode || g_arp->latch_enabled != last_state.latch ||
		    g_tempo->current_bpm != last_state.bpm ||
		    g_arp->base.playing_pitch != last_state.base_pitch ||
		    g_arp->high.playing_pitch != last_state.high_pitch ||
		    g_arp->num_notes != last_state.num_notes ||
		    g_arp->high.length_offset != last_state.poly_offset ||
		    g_arp->drift_cycle != last_state.drift_cycle || current_p != last_state.param ||
		    current_s != last_state.shifted || current_m != last_state.pending_mode) {

			/* Copy state into local snapshot */
			last_state.mode = g_arp->mode;
			last_state.latch = g_arp->latch_enabled;
			last_state.bpm = g_tempo->current_bpm;
			last_state.base_pitch = g_arp->base.playing_pitch;
			last_state.high_pitch = g_arp->high.playing_pitch;
			last_state.num_notes = g_arp->num_notes;
			last_state.poly_offset = g_arp->high.length_offset;
			last_state.drift_cycle = g_arp->drift_cycle;
			last_state.param = current_p;
			last_state.shifted = current_s;
			last_state.pending_mode = current_m;
			changed = true;
		}

		k_mutex_unlock(&g_arp->lock);

		/* --- Step 2: Render if changed --- */
		if (changed) {
			const char *m_str = "SYNC";
			enum arp_mode disp_m = (last_state.param == ENCODER_PARAM_MODE)
						       ? last_state.pending_mode
						       : last_state.mode;

			if (disp_m == ARP_MODE_PHASE) {
				m_str = "PHASE";
			} else if (disp_m == ARP_MODE_PROCESS) {
				m_str = "PROC";
			} else if (disp_m == ARP_MODE_PHASE_PROCESS) {
				m_str = "PH+PR";
			}

			/* Show '?' if the displayed mode is different from active mode */
			char confirm_char = (last_state.param == ENCODER_PARAM_MODE &&
					     last_state.pending_mode != last_state.mode)
						    ? '?'
						    : ' ';

			snprintf(line1, sizeof(line1), "%c%-7s%c %s",
				 (last_state.param == ENCODER_PARAM_MODE ? '*' : ' '), m_str,
				 confirm_char, last_state.latch ? "LATCH" : "     ");

			/* Label changes from BPM to bpm when shifted (Fine mode) */
			snprintf(line2, sizeof(line2), "%c%s: %d.%02d  %s",
				 (last_state.param == ENCODER_PARAM_BPM ? '*' : ' '),
				 (last_state.shifted ? "bpm" : "BPM"), last_state.bpm / 100,
				 last_state.bpm % 100, (last_state.shifted ? "SHIFT" : "     "));

			snprintf(line3, sizeof(line3), "%cDrift: %d",
				 (last_state.param == ENCODER_PARAM_DRIFT ? '*' : ' '),
				 last_state.drift_cycle);

			/* Line 4: Share space between Notes and Poly Offset */
			snprintf(line4, sizeof(line4), "%cPoly: %2d  (N:%d)",
				 (last_state.param == ENCODER_PARAM_POLY_OFFSET ? '*' : ' '),
				 last_state.poly_offset, last_state.num_notes);

			/* Perform I2C transfer outside of mutex lock */
			cfb_framebuffer_clear(display_dev, false);
			cfb_print(display_dev, line1, 0, 0);
			cfb_print(display_dev, line2, 0, font_height);
			cfb_print(display_dev, line3, 0, font_height * 2);
			cfb_print(display_dev, line4, 0, font_height * 3);
			cfb_framebuffer_finalize(display_dev);
		}

		/* Poll every 100ms */
		k_msleep(100);
	}
}

int display_mgr_init(struct arp_ctx *arp, struct tempo_ctx *tempo)
{
	g_arp = arp;
	g_tempo = tempo;
	display_dev = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_display));

	if (display_dev != NULL && device_is_ready(display_dev)) {
		if (cfb_framebuffer_init(display_dev)) {
			LOG_ERR("Framebuffer initialization failed!");
			return -ENODEV;
		}

		cfb_framebuffer_clear(display_dev, true);
		cfb_print(display_dev, "Arp Modular", 0, 0);
		cfb_framebuffer_finalize(display_dev);

		/* Start background thread with lowest priority */
		k_thread_create(&display_thread_data, display_stack,
				K_THREAD_STACK_SIZEOF(display_stack), display_thread, NULL, NULL,
				NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
		k_thread_name_set(&display_thread_data, "display_mgr");
	}

	return 0;
}
