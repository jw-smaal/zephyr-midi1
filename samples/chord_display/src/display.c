/*
 * SPDX-License-Identifier: Apache-2.0
 */
 
/*
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @file display.c
 * @date 20260504
 */ 
#include "display.h"

LOG_MODULE_REGISTER(display, CONFIG_LOG_DEFAULT_LEVEL);
/*
 *  GLOBALS
 */
static lv_obj_t *chord_label;
static struct k_mutex g_model_lock;
static char display_chord_name[64];
static bool display_needs_update;

static void init_display(void) 
{
	k_mutex_init(&g_model_lock);
	display_needs_update = false;

        /* Screen background (solid black, no transparency) */
	lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

	/* Default text: white, size  */
	lv_obj_set_style_text_color(lv_screen_active(), lv_color_white(), LV_PART_MAIN);
	lv_obj_set_style_text_font(lv_screen_active(), &lv_font_montserrat_48, LV_PART_MAIN);
	/* =====================================================
	 *  TOP BAR
	 * ===================================================== */
	/* Left: static title */
	chord_label = lv_label_create(lv_screen_active());
	lv_label_set_text(chord_label, "Chord Display");
	lv_obj_set_width(chord_label, lv_pct(90));
	lv_obj_set_style_text_align(chord_label, LV_TEXT_ALIGN_CENTER, 0);
	lv_label_set_long_mode(chord_label, LV_LABEL_LONG_WRAP);
	lv_obj_align(chord_label, LV_ALIGN_CENTER, 0, 0);     
    	LOG_INF("init done");  
}

void update_chord_display(char * buf, uint8_t len){
	k_mutex_lock(&g_model_lock, K_FOREVER);
	snprintf(display_chord_name, sizeof(display_chord_name), "%s", buf);
	display_needs_update = true;
	k_mutex_unlock(&g_model_lock);
}

void lvgl_thread(void)
{
        LOG_INF("Starting LVGL thread"); 

	const struct device *display_dev;
        
	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return;
	}

	init_display();
        LOG_INF("Starting LVGL loop");   
        uint32_t sleep_ms = 0;
	while (1) {
		k_mutex_lock(&g_model_lock, K_FOREVER);
		if (display_needs_update) {
			lv_label_set_text(chord_label, display_chord_name);
			display_needs_update = false;
		}
		k_mutex_unlock(&g_model_lock);

		sleep_ms = lv_timer_handler();
		k_msleep(sleep_ms);
	}
	return;
}

/* For LVGL had to increase the stack size */
K_THREAD_DEFINE(lvgl_thread_tid, 8192, lvgl_thread, NULL, NULL, NULL, 5, 0, 0);

/* EOF */
