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

LOG_MODULE_REGISTER(display_mgr, LOG_LEVEL_WRN);

/* Stack for the background display thread */
K_THREAD_STACK_DEFINE(display_stack, 2048);
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
	} last_state = {.mode = 255};

	char line1[32], line2[32], line3[32], line4[32];
	uint8_t font_height;

	if (display_dev == NULL || !device_is_ready(display_dev)) {
		return;
	}

	cfb_get_font_size(display_dev, 0, NULL, &font_height);

	while (1) {
		bool changed = false;

		/* --- Step 1: Rapid Snapshot --- */
		k_mutex_lock(&g_arp->lock, K_FOREVER);

		if (g_arp->mode != last_state.mode || g_arp->latch_enabled != last_state.latch ||
		    g_tempo->last_bpm != last_state.bpm ||
		    g_arp->base.playing_pitch != last_state.base_pitch ||
		    g_arp->high.playing_pitch != last_state.high_pitch ||
		    g_arp->num_notes != last_state.num_notes) {

			/* Copy state into local snapshot */
			last_state.mode = g_arp->mode;
			last_state.latch = g_arp->latch_enabled;
			last_state.bpm = g_tempo->last_bpm;
			last_state.base_pitch = g_arp->base.playing_pitch;
			last_state.high_pitch = g_arp->high.playing_pitch;
			last_state.num_notes = g_arp->num_notes;
			changed = true;
		}

		k_mutex_unlock(&g_arp->lock);

		/* --- Step 2: Render if changed --- */
		if (changed) {
			const char *m_str = "SYNC";
			if (last_state.mode == ARP_MODE_PHASE) {
				m_str = "PHASE";
			} else if (last_state.mode == ARP_MODE_PROCESS) {
				m_str = "PROC";
			} else if (last_state.mode == ARP_MODE_PHASE_PROCESS) {
				m_str = "PH+PR";
			}

			snprintf(line1, sizeof(line1), "M:%s L:%s", m_str,
				 last_state.latch ? "ON" : "OFF");
			snprintf(line2, sizeof(line2), "BPM: %d.%02d", last_state.bpm / 100,
				 last_state.bpm % 100);

			if (last_state.num_notes > 0) {
				snprintf(line3, sizeof(line3), "Notes: %d", last_state.num_notes);
			} else {
				snprintf(line3, sizeof(line3), "IDLE");
			}

			/* Perform I2C transfer outside of mutex lock */
			cfb_framebuffer_clear(display_dev, false);
			cfb_print(display_dev, line1, 0, 0);
			cfb_print(display_dev, line2, 0, font_height);
			cfb_print(display_dev, line3, 0, font_height * 2);
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
