/*
 * main.c - USB HID Joystick to MIDI Bridge Entry Point
 *
 * Copyright 2026 NXP
 * @author Jan-Willem Smaal <usenet@gispen.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/usb/usbh.h>
#include <zephyr/logging/log.h>

#include "midi_bridge.h"
#include "joystick_hid.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* USB Host Context */
USBH_CONTROLLER_DEFINE(my_uhs_ctx, DEVICE_DT_GET(DT_CHOSEN(zephyr_uhc0)));

/* Register the Joystick HID Class Driver */
JOYSTICK_HID_CLASS_DEFINE(joystick_host);

int main(void)
{
	int ret;

	LOG_INF("Starting USB HID Joystick Host sample");

	/* 1. Initialize MIDI Bridge */
	ret = midi_bridge_init();
	if (ret != 0) {
		LOG_ERR("Failed to initialize MIDI bridge (%d)", ret);
		/* Continue anyway to allow USB debugging */
	}

	/* 2. Initialize USB Host Stack */
	ret = usbh_init(&my_uhs_ctx);
	if (ret != 0) {
		LOG_ERR("Failed to initialize USB host (%d)", ret);
		return 0;
	}

	/* 3. Enable USB Host Stack */
	ret = usbh_enable(&my_uhs_ctx);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB host (%d)", ret);
		return 0;
	}

	LOG_INF("USB host stack enabled. Waiting for device...");

	while (1) {
		k_sleep(K_MSEC(1000));
	}

	return 0;
}
