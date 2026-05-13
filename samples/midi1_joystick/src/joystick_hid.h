/*
 * joystick_hid.h - USB Host HID Class Driver for T.16000M
 *
 * Copyright 2026 NXP
 * @author Jan-Willem Smaal <usenet@gispen.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef JOYSTICK_HID_H
#define JOYSTICK_HID_H

#include <zephyr/usb/usbh.h>

/**
 * @brief Macro to define the joystick HID class driver
 * 
 * This should be used in the main.c file to register the driver
 * with the USB host stack.
 */
#define JOYSTICK_HID_CLASS_DEFINE(name) \
	USBH_DEFINE_CLASS(name, &hid_host_class_api, &joystick_hid_data, NULL)

extern struct usbh_class_api hid_host_class_api;
extern struct hid_host_data joystick_hid_data;

#endif /* JOYSTICK_HID_H */
