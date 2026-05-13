/*
 * Copyright 2026 NXP
 * @author Jan-Willem Smaal <usenet@gispen.org>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/usb/usbh.h>
#include <zephyr/usb/usb_ch9.h>
#include <zephyr/usb/class/hid.h>
#include <zephyr/drivers/usb/uhc.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/logging/log.h>

/* Include internal descriptor helpers */
#include "../../../../../zephyr/subsys/usb/host/usbh_desc.h"
#include "../../../../../zephyr/subsys/usb/host/usbh_ch9.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* MIDI device */
static const struct device *midi_dev;

/* Internal macros normally in usbh_class.h */
#define USBH_CLASS_MATCH_CODE_TRIPLE BIT(2)
#define USBH_CLASS_IFNUM_DEVICE 0xff

typedef int (*usbh_udev_cb_t)(struct usb_device *const udev,
			      struct uhc_transfer *const xfer);

/* USB Host Context */
USBH_CONTROLLER_DEFINE(my_uhs_ctx, DEVICE_DT_GET(DT_CHOSEN(zephyr_uhc0)));

/* HID Host data */
struct hid_host_data {
	struct usb_device *udev;
	uint8_t ep_in;
	uint16_t mps_in;
	struct uhc_transfer *xfer_in;
	uint16_t last_buttons;
	uint8_t last_fader;
};

static struct hid_host_data joystick_data;

/**
 * @brief Fetch and print the HID Report Descriptor for verification
 */
static void dump_report_descriptor(struct usb_device *const udev, uint8_t iface)
{
	struct net_buf *buf;
	int ret;

	buf = usbh_xfer_buf_alloc(udev, 256);
	if (buf != NULL) {
		/* HID Report Descriptor Type = 0x22 */
		ret = usbh_req_desc(udev, 0x22, 0, iface, 256, buf);
		if (ret == 0) {
			printk("--- RAW HID REPORT DESCRIPTOR (IF %u) ---\n", iface);
			for (int i = 0; i < buf->len; i++) {
				printk("%02x ", buf->data[i]);
				if ((i + 1) % 16 == 0) {
					printk("\n");
				}
			}
			printk("\n---------------------------------------\n");
		}
		uhc_xfer_buf_free(my_uhs_ctx.dev, buf);
	}
}

/* Transfer completion callback */
static int hid_host_udev_cb(struct usb_device *const udev,
			    struct uhc_transfer *const xfer)
{
	struct usbh_context *uhs_ctx_ptr = udev->ctx;
	const struct device *uhc_dev = uhs_ctx_ptr->dev;
	struct hid_host_data *data = &joystick_data;

	if (xfer->err == 0) {
		if (xfer->ep == data->ep_in) {
			if (xfer->buf != NULL && xfer->buf->len >= 9) {
				/* T.16000M mapping:
				 * Byte 0-1: Buttons
				 * Byte 2:   Hat/More buttons
				 * Byte 3-4: X (14-bit LE)
				 * Byte 5-6: Y (14-bit LE)
				 * Byte 7:   Twist (8-bit)
				 * Byte 8:   Fader (8-bit)
				 */
				uint16_t buttons = sys_get_le16(&xfer->buf->data[0]);
				uint16_t x_raw = sys_get_le16(&xfer->buf->data[3]) & 0x3FFF;
				uint16_t y_raw = sys_get_le16(&xfer->buf->data[5]) & 0x3FFF;
				uint8_t fader = xfer->buf->data[8];

				if (device_is_ready(midi_dev)) {
					/* 1. X/Y to Pitch/Mod */
					midi1_serial_pitchwheel(midi_dev, 0, x_raw);
					midi1_serial_control_change(midi_dev, 0, 1, y_raw >> 7);
					
					printk("STICK X:%-5u Y:%-5u | MIDI -> PW:%-5u MOD:%-3u\n", 
						x_raw, y_raw, x_raw, y_raw >> 7);

					/* 2. Fader to Volume (CC 7) */
					if (fader != data->last_fader) {
						midi1_serial_control_change(midi_dev, 0, 7, fader >> 1);
						printk("STICK FADER:%-3u    | MIDI -> VOL:%-3u\n", fader, fader >> 1);
						data->last_fader = fader;
					}

					/* 3. Fire Button (Bit 0) to Note 60 */
					if ((buttons & BIT(0)) != (data->last_buttons & BIT(0))) {
						if (buttons & BIT(0)) {
							midi1_serial_note_on(midi_dev, 0, 60, 100);
							printk("STICK FIRE:DOWN    | MIDI -> NOTE 60 ON\n");
						} else {
							midi1_serial_note_off(midi_dev, 0, 60, 100);
							printk("STICK FIRE:UP      | MIDI -> NOTE 60 OFF\n");
						}
					}
					data->last_buttons = buttons;
				} else {
					/* Fallback for when MIDI gear is not ready */
					printk("STICK X:%-5u Y:%-5u (MIDI Device NOT READY)\n", x_raw, y_raw);
				}
			}
		}
	} else {
		LOG_ERR("Transfer failed: %d", xfer->err);
		k_sleep(K_MSEC(10));
	}

	/* resubmit always */
	if (xfer->buf != NULL) {
		net_buf_reset(xfer->buf);
	}
	if (uhc_ep_enqueue(uhc_dev, xfer) != 0) {
		LOG_ERR("Failed to resubmit IN transfer");
	}

	return 0;
}

/* Device probe callback */
static int hid_host_probe(struct usbh_class_data *const c_data,
			  struct usb_device *const udev,
			  const uint8_t iface)
{
	struct hid_host_data *data = c_data->priv;
	struct usbh_context *uhs_ctx_ptr = udev->ctx;
	const struct device *uhc_dev = uhs_ctx_ptr->dev;

	if (iface == USBH_CLASS_IFNUM_DEVICE) {
		return -ENOTSUP;
	}

	printk("Probing device VID=0x%04x PID=0x%04x interface %u\n",
		udev->dev_desc.idVendor, udev->dev_desc.idProduct, iface);
	
	const struct usb_if_descriptor *if_desc = (void *)usbh_desc_get_iface(udev, iface);
	if (if_desc) {
		printk("  Interface Class: 0x%02x Subclass: 0x%02x Protocol: 0x%02x\n",
			if_desc->bInterfaceClass, if_desc->bInterfaceSubClass, if_desc->bInterfaceProtocol);
	}

	/* Fetch and dump the report map for verification */
	dump_report_descriptor(udev, iface);

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
		LOG_WRN("No Interrupt IN endpoint found");
		return -ENOTSUP;
	}

	/* Allocate and submit initial transfer */
	data->xfer_in = uhc_xfer_alloc_with_buf(uhc_dev, data->ep_in, udev,
					       hid_host_udev_cb, data,
					       data->mps_in);
	if (data->xfer_in == NULL) {
		LOG_ERR("Failed to allocate IN transfer");
		return -ENOMEM;
	}

	if (uhc_ep_enqueue(uhc_dev, data->xfer_in) != 0) {
		LOG_ERR("Failed to submit initial IN transfer");
		uhc_xfer_free(uhc_dev, data->xfer_in);
		return -EIO;
	}

	return 0;
}

static int hid_host_removed(struct usbh_class_data *const c_data)
{
	struct hid_host_data *data = c_data->priv;

	LOG_INF("HID interface removed");
	data->udev = NULL;
	return 0;
}

static int hid_host_init(struct usbh_class_data *const c_data)
{
	return 0;
}

static struct usbh_class_api hid_host_class_api = {
	.init = hid_host_init,
	.probe = hid_host_probe,
	.removed = hid_host_removed,
};

USBH_DEFINE_CLASS(joystick_host, &hid_host_class_api, &joystick_data, NULL);

int main(void)
{
	int ret;

	LOG_INF("Starting USB HID Joystick Host sample");

	midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi0));
	if (device_is_ready(midi_dev)) {
		printk("MIDI device %s is ready\n", midi_dev->name);
	} else {
		LOG_ERR("MIDI device not ready");
	}

	ret = usbh_init(&my_uhs_ctx);
	if (ret != 0) {
		LOG_ERR("Failed to initialize USB host (%d)", ret);
		return 0;
	}

	ret = usbh_enable(&my_uhs_ctx);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB host (%d)", ret);
		return 0;
	}

	while (1) {
		k_sleep(K_MSEC(1000));
	}

	return 0;
}
