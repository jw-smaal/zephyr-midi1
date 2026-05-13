/*
 * joystick_hid.c - USB Host HID Class Driver for T.16000M
 *
 * Copyright 2026 NXP
 * @author Jan-Willem Smaal <usenet@gispen.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usbh.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/drivers/usb/uhc.h>
#include <zephyr/logging/log.h>

#include "midi_bridge.h"
#include "joystick_hid.h"

LOG_MODULE_DECLARE(main, LOG_LEVEL_INF);

/* HID Host private data */
struct hid_host_data {
	struct usb_device *udev;
	uint8_t ep_in;
	uint16_t mps_in;
	struct uhc_transfer *xfer_in;
};

struct hid_host_data joystick_hid_data;

static int hid_host_udev_cb(struct usb_device *const udev,
			    struct uhc_transfer *const xfer)
{
	struct usbh_context *uhs_ctx_ptr = udev->ctx;
	const struct device *uhc_dev = uhs_ctx_ptr->dev;
	struct hid_host_data *data = &joystick_hid_data;

	if (xfer->err == 0) {
		if (xfer->ep == data->ep_in) {
			if (xfer->buf != NULL && xfer->buf->len >= 9) {
				struct joystick_state state;

				/* T.16000M mapping:
				 * Byte 0-1: Buttons
				 * Byte 3-4: X (14-bit LE)
				 * Byte 5-6: Y (14-bit LE)
				 * Byte 7:   Twist (8-bit)
				 * Byte 8:   Fader (8-bit)
				 */
				state.buttons = sys_get_le16(&xfer->buf->data[0]);
				state.x = sys_get_le16(&xfer->buf->data[3]) & 0x3FFF;
				state.y = sys_get_le16(&xfer->buf->data[5]) & 0x3FFF;
				state.twist = xfer->buf->data[7];
				state.fader = xfer->buf->data[8];

				/* Call bridge to process high-level logic */
				midi_bridge_process(&state);
			}
		}
	} else {
		LOG_ERR("USB Transfer failed: %d", xfer->err);
		k_sleep(K_MSEC(10));
	}

	/* Always resubmit to keep polling */
	if (xfer->buf != NULL) {
		net_buf_reset(xfer->buf);
	}

	if (uhc_ep_enqueue(uhc_dev, xfer) != 0) {
		LOG_ERR("Failed to resubmit IN transfer");
	}

	return 0;
}

static int hid_host_probe(struct usbh_class_data *const c_data,
			  struct usb_device *const udev,
			  const uint8_t iface)
{
	struct hid_host_data *data = c_data->priv;
	struct usbh_context *uhs_ctx = udev->ctx;
	const struct device *uhc_dev = uhs_ctx->dev;

	/* Skip device-level probe, we only care about interfaces */
	if (iface == 0xFF) {
		return -ENOTSUP;
	}

	LOG_INF("Probing Joystick VID=0x%04x PID=0x%04x (Interface %u)",
	        udev->dev_desc.idVendor, udev->dev_desc.idProduct, iface);

	data->udev = udev;
	data->ep_in = 0;

	/* Find Interrupt IN endpoint */
	for (int i = 0; i < 16; i++) {
		if (udev->ep_in[i].desc != NULL &&
		    (udev->ep_in[i].desc->bmAttributes & USB_EP_TRANSFER_TYPE_MASK) ==
		    USB_EP_TYPE_INTERRUPT) {
			data->ep_in = udev->ep_in[i].desc->bEndpointAddress;
			data->mps_in = sys_le16_to_cpu(udev->ep_in[i].desc->wMaxPacketSize);
			LOG_INF("Found Interrupt IN endpoint 0x%02x, MPS %u", data->ep_in, data->mps_in);
			break;
		}
	}

	if (data->ep_in == 0) {
		LOG_WRN("No Interrupt IN endpoint found for HID interface");
		return -ENOTSUP;
	}

	data->xfer_in = uhc_xfer_alloc_with_buf(uhc_dev, data->ep_in, udev,
					       hid_host_udev_cb, data,
					       data->mps_in);
	if (data->xfer_in == NULL) {
		LOG_ERR("Failed to allocate USB transfer buffer");
		return -ENOMEM;
	}

	if (uhc_ep_enqueue(uhc_dev, data->xfer_in) != 0) {
		LOG_ERR("Failed to submit initial USB transfer");
		uhc_xfer_free(uhc_dev, data->xfer_in);
		return -EIO;
	}

	return 0;
}

static int hid_host_removed(struct usbh_class_data *const c_data)
{
	struct hid_host_data *data = c_data->priv;
	LOG_INF("Joystick disconnected");
	data->udev = NULL;
	return 0;
}

static int hid_host_init(struct usbh_class_data *const c_data)
{
	return 0;
}

struct usbh_class_api hid_host_class_api = {
	.init = hid_host_init,
	.probe = hid_host_probe,
	.removed = hid_host_removed,
};
