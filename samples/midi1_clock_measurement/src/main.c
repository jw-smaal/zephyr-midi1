/**
 * @file main.c
 * @brief MIDI 1.0 Clock Measurement Sample Application
 *
 * This sample demonstrates how to use the midi1_clock_meas_cntr driver
 * to measure the BPM of an incoming MIDI timing clock signal.
 * It uses midi1_serial to receive the 0xF8 bytes and pulse the driver.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>
#include <zephyr/drivers/midi/midi1_clock_meas_cntr.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(midi1_clock_meas_sample, LOG_LEVEL_INF);

static const struct device *meas_dev;

/* Callback when a MIDI Realtime message is received */
static void midi1_realtime_callback(uint8_t msg)
{
	if (msg == RT_TIMING_CLOCK) {
		/* Notify the measurement driver that a pulse arrived */
		midi1_clock_meas_cntr_pulse(meas_dev);
	}
}

int main(void)
{
	const struct device *midi_serial = DEVICE_DT_GET_ANY(midi1_serial);
	char bpm_str[16];
	struct midi1_serial_callbacks cb = {0};

	meas_dev = DEVICE_DT_GET_ANY(midi1_clock_meas_cntr);

	if (meas_dev == NULL || midi_serial == NULL) {
		LOG_ERR("Required devices not found in devicetree");
		return -ENODEV;
	}

	if (!device_is_ready(meas_dev)) {
		LOG_ERR("MIDI measurement device %s is not ready", meas_dev->name);
		return -ENODEV;
	}

	if (!device_is_ready(midi_serial)) {
		LOG_ERR("MIDI serial device %s is not ready", midi_serial->name);
		return -ENODEV;
	}

	/* Register real-time callback to catch the 0xF8 bytes */
	cb.realtime = midi1_realtime_callback;
	midi1_serial_register_callbacks(midi_serial, &cb);

	LOG_INF("Starting MIDI Clock Measurement...");

	int64_t last_log_time = 0;

	/* Start a thread or loop for the parser */
	while (1) {
		/* This is a blocking call that parses incoming MIDI bytes */
		midi1_serial_receiveparser(midi_serial);

		int64_t now = k_uptime_get();
		if (now - last_log_time >= 1000) {
			if (midi1_clock_meas_cntr_is_valid(meas_dev)) {
				uint16_t current_bpm = midi1_clock_meas_cntr_get_sbpm(meas_dev);
				sbpm_to_str(current_bpm, bpm_str, sizeof(bpm_str));
				LOG_INF("Incoming Clock: %s BPM", bpm_str);
			} else {
				LOG_INF("Waiting for MIDI clock pulses...");
			}
			last_log_time = now;
		}
	}

	return 0;
}
