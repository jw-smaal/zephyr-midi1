/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_midi2.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <ump_stream_responder.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(midi1_usb_bridge, LOG_LEVEL_INF);

/* 
 * Bridging Logic:
 * USB (UMP) -> MIDI1.0 Serial (Byte stream)
 * MIDI1.0 Serial (Byte stream) -> USB (UMP)
 */

#define USB_MIDI_DT_NODE    DT_NODELABEL(usb_midi)
#define SERIAL_MIDI_DT_NODE DT_NODELABEL(midi1_serial0)

static const struct device *const usb_midi_dev = DEVICE_DT_GET(USB_MIDI_DT_NODE);
static const struct device *const serial_midi_dev = DEVICE_DT_GET(SERIAL_MIDI_DT_NODE);

/* USB Device setup without sample helpers */
USBD_DEVICE_DEFINE(my_usbd, DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)), 0x2fe3, 0x0001);
USBD_DESC_LANG_DEFINE(my_lang);
USBD_DESC_STRING_DEFINE(my_manufacturer, "Zephyr", USBD_DUT_STRING_MANUFACTURER);
USBD_DESC_STRING_DEFINE(my_product, "USB-MIDI1.0 Bridge", USBD_DUT_STRING_PRODUCT);
USBD_CONFIGURATION_DEFINE(my_config, USB_SCD_SELF_POWERED, 100, NULL);

static const struct ump_endpoint_dt_spec ump_ep_dt =
	UMP_ENDPOINT_DT_SPEC_GET(USB_MIDI_DT_NODE);

static struct ump_stream_responder_cfg responder_cfg;


/* --- USB -> Serial Bridge --- */
static void on_usb_rx_packet(const struct device *dev, const struct midi_ump ump)
{
	LOG_DBG("USB -> Serial (MT=%X)", UMP_MT(ump));

	switch (UMP_MT(ump)) {
	case UMP_MT_SYS_RT_COMMON:
	case UMP_MT_MIDI1_CHANNEL_VOICE:
	case UMP_MT_DATA_64:
		/* These can be unrolled to MIDI 1.0 bytes */
		midi1_serial_send_ump(serial_midi_dev, ump);
		break;
	case UMP_MT_UMP_STREAM:
		ump_stream_respond(&responder_cfg, ump);
		break;
	default:
		/* MIDI 2.0 messages (MT=4) etc are not supported by MIDI 1.0 Serial */
		break;
	}
}

static const struct usbd_midi_ops usb_ops = {
	.rx_packet_cb = on_usb_rx_packet,
};


/* --- Serial -> USB Bridge --- */
static void on_serial_ump_cb(const struct device *dev, const struct midi_ump ump)
{
	LOG_DBG("Serial -> USB (MT=%X)", UMP_MT(ump));
	usbd_midi_send(usb_midi_dev, ump);
}

static struct midi1_serial_callbacks serial_callbacks = {
	.ump_cb = on_serial_ump_cb,
};


/* --- Serial Parser Thread --- */
K_THREAD_STACK_DEFINE(parser_stack, 1024);
static struct k_thread parser_thread_data;

void parser_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Serial MIDI parser thread started");

	while (1) {
		/* This is a blocking call if no data is in msgq */
		midi1_serial_receiveparser_ump(serial_midi_dev);
	}
}

int main(void)
{
	int err;

	LOG_INF("Starting MIDI USB Bridge Sample");

	if (!device_is_ready(usb_midi_dev)) {
		LOG_ERR("USB MIDI device not ready");
		return -1;
	}

	if (!device_is_ready(serial_midi_dev)) {
		LOG_ERR("Serial MIDI device not ready");
		return -1;
	}

	/* Initialize responder config */
	responder_cfg.dev = usb_midi_dev;
	responder_cfg.send_cb = usbd_midi_send;
	responder_cfg.ep_dt = &ump_ep_dt;

	/* Register USB callbacks */
	usbd_midi_set_ops(usb_midi_dev, &usb_ops);

	/* Register Serial callbacks */
	midi1_serial_register_callbacks(serial_midi_dev, &serial_callbacks);

	/* Manual USB initialization */
	err = usbd_add_descriptor(&my_usbd, &my_lang);
	err |= usbd_add_descriptor(&my_usbd, &my_manufacturer);
	err |= usbd_add_descriptor(&my_usbd, &my_product);
	err |= usbd_add_configuration(&my_usbd, USBD_SPEED_FS, &my_config);
	/* Need to find correct class instance name or use a macro */
	err |= usbd_register_class(&my_usbd, "midi_0", USBD_SPEED_FS, 1);

	if (err) {
		LOG_ERR("Failed to setup USB descriptors/classes (%d)", err);
		return -1;
	}

	err = usbd_init(&my_usbd);
	if (err) {
		LOG_ERR("Failed to initialize USB device (%d)", err);
		return -1;
	}

	err = usbd_enable(&my_usbd);
	if (err) {
		LOG_ERR("Failed to enable USB device support (%d)", err);
		return -1;
	}

	/* Start parser thread */
	k_thread_create(&parser_thread_data, parser_stack,
			K_THREAD_STACK_SIZEOF(parser_stack),
			parser_thread, NULL, NULL, NULL,
			K_PRIO_COOP(7), 0, K_NO_WAIT);

	LOG_INF("USB MIDI Bridge initialized");

	return 0;
}
