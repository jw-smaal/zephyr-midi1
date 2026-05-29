/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file midi1_serial.c
 * @brief MIDI USART implementation on Zephyr RTOS
 *
 * @note
 * MIDI USART implementation that also implements the MIDI1.0
 * "running status" on transmit (optional) and receive (manditory).
 *
 * The implementation intends to be as complete and close to the MIDI1.0
 * specifications as possible.
 *
 * Originally created in 2014 for AVR MCU's ported to Zephyr RTOS in 2024.
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * updated 20241224
 * updated 20260103
 */
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/sys/atomic.h>

#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>

/*
 * Empty NO OP (noop) callbacks assigned if the caller leaves the callbacks
 * empty.  Gives the caller flexibillity in what to respond to.
 */
static inline void midi1_noop_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
}

static inline void midi1_noop_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
}

static inline void midi1_noop_control_change(uint8_t channel, uint8_t controller, uint8_t value)
{
}

static inline void midi1_noop_pitchwheel(uint8_t channel, uint8_t lsb, uint8_t msb)
{
}

static inline void midi1_noop_program_change(uint8_t channel, uint8_t number)
{
}

static inline void midi1_noop_channel_aftertouch(uint8_t channel, uint8_t pressure)
{
}

static inline void midi1_noop_poly_aftertouch(uint8_t channel, uint8_t note, uint8_t pressure)
{
}

/* System common */
static inline void midi1_noop_realtime(uint8_t msg)
{
}

static inline void midi1_noop_mtc_quarter_frame(uint8_t data)
{
}

static inline void midi1_noop_song_position(uint16_t pos)
{
}

static inline void midi1_noop_song_select(uint8_t song)
{
}

static inline void midi1_noop_tune_request(void)
{
}

/* SysEx */
static inline void midi1_noop_sysex_start(void)
{
}

static inline void midi1_noop_sysex_data(uint8_t data)
{
}

static inline void midi1_noop_sysex_stop(void)
{
}

static inline void midi1_noop_ump_cb(const struct device *dev, const struct midi_ump ump)
{
}

/**
 * @brief MIDI Byte Receive parser implementation - Interrupt Service Routine
 *
 * @param device device pointer
 * @param user_data user data (in our case the instance struct)
 * @note we use a Message Queue FIFO buffer to store the incoming MIDI messages
 */
#ifdef CONFIG_ZTEST
void midi1_serial_isr_callback(const struct device *dev, void *user_data)
#else
static void midi1_serial_isr_callback(const struct device *dev, void *user_data)
#endif
{
	uint8_t c;
	const struct device *midi1_serial = (const struct device *)user_data;
	const struct midi1_serial_config *cfg = midi1_serial->config;
	struct midi1_serial_data *data = midi1_serial->data;

	uart_irq_update(cfg->uart);

	/* Handle TX: Hardware is ready for more data */
	if (uart_irq_tx_ready(cfg->uart)) {
		uint8_t tx_byte;

		/* Check High-Priority Real-Time buffer first */
		if (ring_buf_get(&data->tx_rt_ringbuf, &tx_byte, 1) > 0) {
			uart_fifo_fill(cfg->uart, &tx_byte, 1);
		}
		/* If empty, fall back to normal buffer */
		else if (ring_buf_get(&data->tx_ringbuf, &tx_byte, 1) > 0) {
			uart_fifo_fill(cfg->uart, &tx_byte, 1);
		}
		/* No more data to send, silence the interrupt */
		else {
			uart_irq_tx_disable(cfg->uart);
		}
	}

	/* Handle RX: Data received from hardware */
	if (uart_irq_rx_ready(cfg->uart)) {
		/*
		 * read until FIFO empty
		 */
		while (uart_fifo_read(cfg->uart, &c, 1) == 1) {
			if (k_msgq_put(&data->msgq, &c, K_NO_WAIT) != 0) {
				/*
				 * Buffer full, message dropped
				 */
				atomic_inc(&data->overrun_count);
			}
		}
	}
}

/* Helper for standard messages (Notes, CC, SysEx) */
static void midi1_serial_tx_enqueue(const struct device *dev, uint8_t byte)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;

	/* Critical section: ensure the ISR doesn't modify the ringbuf while we add to it */
	unsigned int key = irq_lock();

	if (ring_buf_put(&data->tx_ringbuf, &byte, 1) > 0) {
		uart_irq_tx_enable(cfg->uart);
	} else {
		atomic_inc(&data->overrun_count);
	}
	irq_unlock(key);
}

/* Helper for high-priority messages (Clock) - Preserves precision */
static void midi1_serial_tx_immediate(const struct device *dev, uint8_t byte)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;

	unsigned int key = irq_lock();
	if (ring_buf_put(&data->tx_rt_ringbuf, &byte, 1) > 0) {
		uart_irq_tx_enable(cfg->uart);
	} else {
		/* Optional: count RT overruns here */
	}
	irq_unlock(key);
}

/**
 * Inits the serial USART ISR routine so we start processing incoming MIDI.
 */
int midi1_serial_init(const struct device *dev)
{
	const struct midi1_serial_config *cfg = dev->config;
	struct midi1_serial_data *data = dev->data;

	data->running_status_rx = 0;
	data->third_byte_flag = 0;
	data->midi_c2 = 0;
	data->midi_c3 = 0;
	atomic_set(&data->overrun_count, 0);

	data->running_status_tx = 0;
	data->running_status_tx_count = 0;
	data->in_sysex = false;
	data->sysex_ump_count = 0;
	data->sysex_ump_started = false;
	data->last_status_tx_time = k_uptime_get_32() - 500U;

	/*
	 * If a null pointer is given reassign to the NO OP function
	 */
	data->cb.note_on = midi1_noop_note_on;
	data->cb.note_off = midi1_noop_note_off;
	data->cb.control_change = midi1_noop_control_change;
	data->cb.realtime = midi1_noop_realtime;
	data->cb.mtc_quarter_frame = midi1_noop_mtc_quarter_frame;
	data->cb.song_position = midi1_noop_song_position;
	data->cb.song_select = midi1_noop_song_select;
	data->cb.tune_request = midi1_noop_tune_request;
	data->cb.pitchwheel = midi1_noop_pitchwheel;
	data->cb.program_change = midi1_noop_program_change;
	data->cb.channel_aftertouch = midi1_noop_channel_aftertouch;
	data->cb.poly_aftertouch = midi1_noop_poly_aftertouch;
	data->cb.sysex_start = midi1_noop_sysex_start;
	data->cb.sysex_data = midi1_noop_sysex_data;
	data->cb.sysex_stop = midi1_noop_sysex_stop;
	data->cb.ump_cb = midi1_noop_ump_cb;

	/*
	 * Assign a MSQ to this instance
	 */
	ring_buf_init(&data->tx_ringbuf, sizeof(data->tx_buffer), data->tx_buffer);
	ring_buf_init(&data->tx_rt_ringbuf, sizeof(data->tx_rt_buffer), data->tx_rt_buffer);
	k_msgq_init(&data->msgq, data->msgq_buffer, MSG_SIZE, MSGQ_SIZE);
	k_mutex_init(&data->tx_lock);

	if (!device_is_ready(cfg->uart)) {
		return -ENODEV;
	}
	int ret =
		uart_irq_callback_user_data_set(cfg->uart, midi1_serial_isr_callback, (void *)dev);
	if (ret < 0) {
		return ret;
	}

	uart_irq_rx_enable(cfg->uart);
	return 0;
}

int midi1_serial_register_callbacks(const struct device *dev, struct midi1_serial_callbacks *cb)
{
	if (!cb) {
		return -EINVAL;
	}

	struct midi1_serial_data *data = dev->data;

	if (cb->note_on) {
		data->cb.note_on = cb->note_on;
	}
	if (cb->note_off) {
		data->cb.note_off = cb->note_off;
	}
	if (cb->control_change) {
		data->cb.control_change = cb->control_change;
	}
	if (cb->pitchwheel) {
		data->cb.pitchwheel = cb->pitchwheel;
	}
	if (cb->program_change) {
		data->cb.program_change = cb->program_change;
	}
	if (cb->channel_aftertouch) {
		data->cb.channel_aftertouch = cb->channel_aftertouch;
	}
	if (cb->poly_aftertouch) {
		data->cb.poly_aftertouch = cb->poly_aftertouch;
	}
	if (cb->realtime) {
		data->cb.realtime = cb->realtime;
	}
	if (cb->mtc_quarter_frame) {
		data->cb.mtc_quarter_frame = cb->mtc_quarter_frame;
	}
	if (cb->song_position) {
		data->cb.song_position = cb->song_position;
	}
	if (cb->song_select) {
		data->cb.song_select = cb->song_select;
	}
	if (cb->tune_request) {
		data->cb.tune_request = cb->tune_request;
	}
	if (cb->sysex_start) {
		data->cb.sysex_start = cb->sysex_start;
	}
	if (cb->sysex_data) {
		data->cb.sysex_data = cb->sysex_data;
	}
	if (cb->sysex_stop) {
		data->cb.sysex_stop = cb->sysex_stop;
	}
	if (cb->ump_cb) {
		data->cb.ump_cb = cb->ump_cb;
	}

	return 0;
}

#define RUNSTAT_TMOUT CONFIG_MIDI1_SERIAL_RUNNING_STATUS_TIMEOUT_MS
#define RUNSTAT_TIMES CONFIG_MIDI1_SERIAL_RUNNING_STATUS_REPEAT

static bool midi1_need_status(struct midi1_serial_data *data, uint8_t status)
{
	uint32_t now = k_uptime_get_32();

	bool need_status = (now - data->last_status_tx_time > RUNSTAT_TMOUT) ||
			   (status != data->running_status_tx) ||
			   (data->running_status_tx_count >= RUNSTAT_TIMES);

	return need_status;
}

void midi1_serial_note_on(const struct device *dev, uint8_t channel, uint8_t key, uint8_t velocity)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t status = C_NOTE_ON | (channel & 0x0F);

	if (key > 127 || velocity > 127) {
		return;
	}

	k_mutex_lock(&data->tx_lock, K_FOREVER);
	if (midi1_need_status(data, status)) {
		midi1_serial_tx_enqueue(dev, status);
		data->running_status_tx = status;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = k_uptime_get_32();
	}
	midi1_serial_tx_enqueue(dev, key);
	midi1_serial_tx_enqueue(dev, velocity);
	data->running_status_tx_count++;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_note_off(const struct device *dev, uint8_t channel, uint8_t key, uint8_t velocity)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t status = C_NOTE_OFF | (channel & 0x0F);

	if (key > 127 || velocity > 127) {
		return;
	}

	k_mutex_lock(&data->tx_lock, K_FOREVER);
	if (midi1_need_status(data, status)) {
		midi1_serial_tx_enqueue(dev, status);
		data->running_status_tx = status;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = k_uptime_get_32();
	}
	midi1_serial_tx_enqueue(dev, key);
	midi1_serial_tx_enqueue(dev, velocity);
	data->running_status_tx_count++;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_control_change(const struct device *dev, uint8_t channel, uint8_t controller,
				 uint8_t val)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t status = C_CONTROL_CHANGE | (channel & 0x0F);

	if (controller > 127 || val > 127) {
		return;
	}

	k_mutex_lock(&data->tx_lock, K_FOREVER);
	if (midi1_need_status(data, status)) {
		midi1_serial_tx_enqueue(dev, status);
		data->running_status_tx = status;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = k_uptime_get_32();
	}
	midi1_serial_tx_enqueue(dev, controller);
	midi1_serial_tx_enqueue(dev, val);
	data->running_status_tx_count++;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_channelaftertouch(const struct device *dev, uint8_t channel, uint8_t val)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t status = C_CHANNEL_AFTERTOUCH | (channel & 0x0F);

	if (val > 127) {
		return;
	}

	k_mutex_lock(&data->tx_lock, K_FOREVER);
	if (midi1_need_status(data, status)) {
		midi1_serial_tx_enqueue(dev, status);
		data->running_status_tx = status;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = k_uptime_get_32();
	}
	midi1_serial_tx_enqueue(dev, val);
	data->running_status_tx_count++;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_program_change(const struct device *dev, uint8_t channel, uint8_t number)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t status = C_PROGRAM_CHANGE | (channel & 0x0F);

	if (number > 127) {
		return;
	}

	k_mutex_lock(&data->tx_lock, K_FOREVER);
	if (midi1_need_status(data, status)) {
		midi1_serial_tx_enqueue(dev, status);
		data->running_status_tx = status;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = k_uptime_get_32();
	}
	midi1_serial_tx_enqueue(dev, number);
	data->running_status_tx_count++;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_polyaftertouch(const struct device *dev, uint8_t channel, uint8_t key,
				 uint8_t val)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t status = C_POLYPHONIC_AFTERTOUCH | (channel & 0x0F);

	if (key > 127 || val > 127) {
		return;
	}

	k_mutex_lock(&data->tx_lock, K_FOREVER);
	if (midi1_need_status(data, status)) {
		midi1_serial_tx_enqueue(dev, status);
		data->running_status_tx = status;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = k_uptime_get_32();
	}
	midi1_serial_tx_enqueue(dev, key);
	midi1_serial_tx_enqueue(dev, val);
	data->running_status_tx_count++;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_modwheel(const struct device *dev, uint8_t channel, uint16_t val)
{
	struct midi1_serial_data *data = dev->data;
	k_mutex_lock(&data->tx_lock, K_FOREVER);
	midi1_serial_control_change(dev, channel, CTL_MSB_MODWHEEL, (uint8_t)((val >> 7) & 0x7F));
	midi1_serial_control_change(dev, channel, CTL_LSB_MODWHEEL, (uint8_t)(val & 0x7F));
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_pitchwheel(const struct device *dev, uint8_t channel, uint16_t val)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t status = C_PITCH_WHEEL | (channel & 0x0F);

	if (val > 16383) {
		return;
	}

	k_mutex_lock(&data->tx_lock, K_FOREVER);
	if (midi1_need_status(data, status)) {
		midi1_serial_tx_enqueue(dev, status);
		data->running_status_tx = status;
		data->running_status_tx_count = 0;
		data->last_status_tx_time = k_uptime_get_32();
	}
	/*
	 * Value is 14 bits so need to shift 7
	 */
	/*
	 * LSB first
	 */
	midi1_serial_tx_enqueue(dev, (uint8_t)(val & MIDI_DATA));
	/*
	 * then MSB
	 */
	midi1_serial_tx_enqueue(dev, (uint8_t)((val >> 7) & MIDI_DATA));
	data->running_status_tx_count++;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_mtc_quarter_frame(const struct device *dev, uint8_t data_byte)
{
	struct midi1_serial_data *data = dev->data;
	k_mutex_lock(&data->tx_lock, K_FOREVER);
	midi1_serial_tx_enqueue(dev, SYSTEM_MTC_QUARTER_FRAME);
	midi1_serial_tx_enqueue(dev, data_byte & MIDI_DATA);
	data->running_status_tx = 0;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_song_position(const struct device *dev, uint16_t pos)
{
	struct midi1_serial_data *data = dev->data;
	k_mutex_lock(&data->tx_lock, K_FOREVER);
	midi1_serial_tx_enqueue(dev, SYSTEM_SONG_POSITION);
	midi1_serial_tx_enqueue(dev, (uint8_t)(pos & 0x7F));
	midi1_serial_tx_enqueue(dev, (uint8_t)((pos >> 7) & MIDI_DATA));
	data->running_status_tx = 0;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_song_select(const struct device *dev, uint8_t song)
{
	struct midi1_serial_data *data = dev->data;
	k_mutex_lock(&data->tx_lock, K_FOREVER);
	midi1_serial_tx_enqueue(dev, SYSTEM_SONG_SELECT);
	midi1_serial_tx_enqueue(dev, song & MIDI_DATA);
	data->running_status_tx = 0;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_tune_request(const struct device *dev)
{
	struct midi1_serial_data *data = dev->data;
	k_mutex_lock(&data->tx_lock, K_FOREVER);
	midi1_serial_tx_enqueue(dev, SYSTEM_TUNE_REQUEST);
	data->running_status_tx = 0;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_timingclock(const struct device *dev)
{
	midi1_serial_tx_immediate(dev, RT_TIMING_CLOCK);
}

void midi1_serial_start(const struct device *dev)
{
	midi1_serial_tx_immediate(dev, RT_START);
}

void midi1_serial_continue(const struct device *dev)
{
	midi1_serial_tx_immediate(dev, RT_CONTINUE);
}

void midi1_serial_stop(const struct device *dev)
{
	midi1_serial_tx_immediate(dev, RT_STOP);
}

void midi1_serial_active_sensing(const struct device *dev)
{
	midi1_serial_tx_immediate(dev, RT_ACTIVE_SENSING);
}

void midi1_serial_reset(const struct device *dev)
{
	struct midi1_serial_data *data = dev->data;
	k_mutex_lock(&data->tx_lock, K_FOREVER);
	midi1_serial_tx_immediate(dev, RT_RESET);
	data->running_status_tx = 0;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_sysex_start(const struct device *dev)
{
	struct midi1_serial_data *data = dev->data;
	/*
	 * Lock the channel for the entire SysEx transaction to prevent
	 * interleaving of channel/common messages. Real-time messages
	 * bypass this mutex via tx_rt_ringbuf and will still interleave
	 * as per the MIDI 1.0 specification.
	 */
	k_mutex_lock(&data->tx_lock, K_FOREVER);
	midi1_serial_tx_enqueue(dev, SYSTEM_EXCLUSIVE_START);
	data->running_status_tx = 0;
}

void midi1_sysex_char(const struct device *dev, uint8_t c)
{
	/*
	 * Sysex data should be 0 -> 127U so ignore everyhing else.
	 * Mutex is already held by midi1_sysex_start().
	 */
	if (!(c & CHANNEL_VOICE_MASK)) {
		midi1_serial_tx_enqueue(dev, c);
	}
}

void midi1_sysex_data_bulk(const struct device *dev, const uint8_t *data, uint32_t len)
{
	for (uint32_t i = 0; i < len; i++) {
		midi1_sysex_char(dev, data[i]);
	}
}

void midi1_sysex_stop(const struct device *dev)
{
	struct midi1_serial_data *data = dev->data;
	midi1_serial_tx_enqueue(dev, SYSTEM_EXCLUSIVE_END);
	data->running_status_tx = 0;
	k_mutex_unlock(&data->tx_lock);
}

void midi1_serial_receiveparser(const struct device *dev)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t c;

	if (k_msgq_get(&data->msgq, &c, K_FOREVER) != 0) {
		return;
	}

	if (c & 0x80) {
		if (c >= 0xF8) {
			if (c == RT_RESET) {
				data->running_status_rx = 0;
			}
			data->cb.realtime(c);
		} else {
			data->in_sysex = false;
			data->third_byte_flag = 0;
			if (c < 0xF0) {
				data->running_status_rx = c;
			} else {
				data->running_status_rx = 0;
				if (c == SYSTEM_TUNE_REQUEST) {
					data->cb.tune_request();
				} else if (c == SYSTEM_EXCLUSIVE_START) {
					data->in_sysex = true;
					data->cb.sysex_start();
				} else if (c == SYSTEM_EXCLUSIVE_END) {
					data->cb.sysex_stop();
				} else if (c == SYSTEM_MTC_QUARTER_FRAME ||
					   c == SYSTEM_SONG_SELECT || c == SYSTEM_SONG_POSITION) {
					data->running_status_rx = c;
				}
			}
		}
		return;
	}

	if (data->in_sysex) {
		data->cb.sysex_data(c);
		return;
	}

	if (data->third_byte_flag) {
		data->third_byte_flag = 0;
		data->midi_c3 = c;

		if (data->running_status_rx == SYSTEM_SONG_POSITION) {
			data->cb.song_position((data->midi_c3 << 7) | data->midi_c2);
			data->running_status_rx = 0;
			return;
		}

		uint8_t status = data->running_status_rx & 0xF0;
		uint8_t chan = data->running_status_rx & 0x0F;

		if (status == C_NOTE_ON) {
			if (data->midi_c3 == 0) {
				data->cb.note_off(chan, data->midi_c2, 0);
			} else {
				data->cb.note_on(chan, data->midi_c2, data->midi_c3);
			}
		} else if (status == C_NOTE_OFF) {
			data->cb.note_off(chan, data->midi_c2, data->midi_c3);
		} else if (status == C_PITCH_WHEEL) {
			data->cb.pitchwheel(chan, data->midi_c2, data->midi_c3);
		} else if (status == C_POLYPHONIC_AFTERTOUCH) {
			data->cb.poly_aftertouch(chan, data->midi_c2, data->midi_c3);
		} else if (status == C_CONTROL_CHANGE) {
			data->cb.control_change(chan, data->midi_c2, data->midi_c3);
		}
	} else {
		if (data->running_status_rx == 0) {
			return;
		}
		if (data->running_status_rx == SYSTEM_MTC_QUARTER_FRAME) {
			data->cb.mtc_quarter_frame(c);
			data->running_status_rx = 0;
		} else if (data->running_status_rx == SYSTEM_SONG_SELECT) {
			data->cb.song_select(c);
			data->running_status_rx = 0;
		} else if (data->running_status_rx < 0xC0 ||
			   (data->running_status_rx >= 0xE0 && data->running_status_rx < 0xF0) ||
			   data->running_status_rx == SYSTEM_SONG_POSITION) {
			data->midi_c2 = c;
			data->third_byte_flag = 1;
		} else if (data->running_status_rx < 0xE0) {
			uint8_t status = data->running_status_rx & 0xF0;
			uint8_t chan = data->running_status_rx & 0x0F;
			if (status == C_PROGRAM_CHANGE) {
				data->cb.program_change(chan, c);
			} else if (status == C_CHANNEL_AFTERTOUCH) {
				data->cb.channel_aftertouch(chan, c);
			}
		}
	}
}

static void midi1_serial_ump_sysex_flush(const struct device *dev, uint8_t status)
{
	struct midi1_serial_data *data = dev->data;

	if (data->sysex_ump_count > 0 || status == UMP_SYSEX7_STATUS_END ||
	    status == UMP_SYSEX7_STATUS_COMPLETE) {
		data->cb.ump_cb(dev, midi1_sysex7(UMP_CHANNEL_GROUP, status, data->sysex_ump_count,
						  data->sysex_ump_buf));
		data->sysex_ump_count = 0;
		data->sysex_ump_started = true;
	}
}

void midi1_serial_receiveparser_ump(const struct device *dev)
{
	struct midi1_serial_data *data = dev->data;
	uint8_t c;

	if (k_msgq_get(&data->msgq, &c, K_FOREVER) != 0) {
		return;
	}

	if (c & 0x80) {
		if (c >= 0xF8) {
			if (c == RT_RESET) {
				data->running_status_rx = 0;
			}
			data->cb.ump_cb(dev, UMP_SYS_RT_COMMON(UMP_CHANNEL_GROUP, c, 0, 0));
		} else {
			if (data->in_sysex) {
				/* Status byte interrupted SysEx */
				uint8_t status = data->sysex_ump_started
							 ? UMP_SYSEX7_STATUS_END
							 : UMP_SYSEX7_STATUS_COMPLETE;
				midi1_serial_ump_sysex_flush(dev, status);
			}
			data->in_sysex = false;
			data->third_byte_flag = 0;
			if (c < 0xF0) {
				data->running_status_rx = c;
			} else {
				data->running_status_rx = 0;
				if (c == SYSTEM_TUNE_REQUEST) {
					data->cb.ump_cb(dev, UMP_SYS_RT_COMMON(UMP_CHANNEL_GROUP,
									       SYSTEM_TUNE_REQUEST,
									       0, 0));
				} else if (c == SYSTEM_EXCLUSIVE_START) {
					data->in_sysex = true;
					data->sysex_ump_count = 0;
					data->sysex_ump_started = false;
				} else if (c == SYSTEM_EXCLUSIVE_END) {
					if (data->in_sysex) {
						uint8_t status =
							data->sysex_ump_started
								? UMP_SYSEX7_STATUS_END
								: UMP_SYSEX7_STATUS_COMPLETE;
						midi1_serial_ump_sysex_flush(dev, status);
					}
				} else if (c == SYSTEM_MTC_QUARTER_FRAME ||
					   c == SYSTEM_SONG_SELECT || c == SYSTEM_SONG_POSITION) {
					data->running_status_rx = c;
				}
			}
		}
		return;
	}

	if (data->in_sysex) {
		data->sysex_ump_buf[data->sysex_ump_count++] = c;
		if (data->sysex_ump_count == 6) {
			uint8_t status = data->sysex_ump_started ? UMP_SYSEX7_STATUS_CONTINUE
								 : UMP_SYSEX7_STATUS_START;
			midi1_serial_ump_sysex_flush(dev, status);
		}
		return;
	}

	if (data->third_byte_flag) {
		data->third_byte_flag = 0;
		data->midi_c3 = c;

		if (data->running_status_rx == SYSTEM_SONG_POSITION) {
			data->cb.ump_cb(dev,
					UMP_SYS_RT_COMMON(UMP_CHANNEL_GROUP, SYSTEM_SONG_POSITION,
							  data->midi_c2, data->midi_c3));
			data->running_status_rx = 0;
			return;
		}

		uint8_t status = data->running_status_rx & 0xF0;
		uint8_t chan = data->running_status_rx & 0x0F;

		if (status == C_NOTE_ON) {
			if (data->midi_c3 == 0) {
				data->cb.ump_cb(dev, midi1_note_off(chan, data->midi_c2, 0));
			} else {
				data->cb.ump_cb(dev,
						midi1_note_on(chan, data->midi_c2, data->midi_c3));
			}
		} else if (status == C_NOTE_OFF) {
			data->cb.ump_cb(dev, midi1_note_off(chan, data->midi_c2, data->midi_c3));
		} else if (status == C_PITCH_WHEEL) {
			data->cb.ump_cb(
				dev, midi1_pitchwheel(chan, (data->midi_c3 << 7) | data->midi_c2));
		} else if (status == C_POLYPHONIC_AFTERTOUCH) {
			data->cb.ump_cb(dev,
					midi1_polyaftertouch(chan, data->midi_c2, data->midi_c3));
		} else if (status == C_CONTROL_CHANGE) {
			data->cb.ump_cb(dev,
					midi1_controlchange(chan, data->midi_c2, data->midi_c3));
		}
	} else {
		if (data->running_status_rx == 0) {
			return;
		}
		if (data->running_status_rx == SYSTEM_MTC_QUARTER_FRAME) {
			data->cb.ump_cb(dev, UMP_SYS_RT_COMMON(UMP_CHANNEL_GROUP,
							       SYSTEM_MTC_QUARTER_FRAME, c, 0));
			data->running_status_rx = 0;
		} else if (data->running_status_rx == SYSTEM_SONG_SELECT) {
			data->cb.ump_cb(dev, UMP_SYS_RT_COMMON(UMP_CHANNEL_GROUP,
							       SYSTEM_SONG_SELECT, c, 0));
			data->running_status_rx = 0;
		} else if (data->running_status_rx < 0xC0 ||
			   (data->running_status_rx >= 0xE0 && data->running_status_rx < 0xF0) ||
			   data->running_status_rx == SYSTEM_SONG_POSITION) {
			data->midi_c2 = c;
			data->third_byte_flag = 1;
		} else if (data->running_status_rx < 0xE0) {
			uint8_t status = data->running_status_rx & 0xF0;
			uint8_t chan = data->running_status_rx & 0x0F;
			if (status == C_PROGRAM_CHANGE) {
				data->cb.ump_cb(dev, UMP_MIDI1_CHANNEL_VOICE(
							     UMP_CHANNEL_GROUP,
							     UMP_MIDI_PROGRAM_CHANGE, chan, c, 0));
			} else if (status == C_CHANNEL_AFTERTOUCH) {
				data->cb.ump_cb(dev, midi1_channelaftertouch(chan, c));
			}
		}
	}
}

void midi1_serial_send_ump(const struct device *dev, const struct midi_ump ump)
{
	uint8_t mt = UMP_MT(ump);

	switch (mt) {
	case UMP_MT_SYS_RT_COMMON: {
		uint8_t status = UMP_MIDI_STATUS(ump);
		if (status >= 0xF8) {
			midi1_serial_tx_immediate(dev, status);
		} else {
			k_mutex_lock(&((struct midi1_serial_data *)dev->data)->tx_lock, K_FOREVER);
			midi1_serial_tx_enqueue(dev, status);
			if (status == SYSTEM_MTC_QUARTER_FRAME || status == SYSTEM_SONG_SELECT) {
				midi1_serial_tx_enqueue(dev, UMP_MIDI1_P1(ump));
			} else if (status == SYSTEM_SONG_POSITION) {
				midi1_serial_tx_enqueue(dev, UMP_MIDI1_P1(ump));
				midi1_serial_tx_enqueue(dev, UMP_MIDI1_P2(ump));
			}
			((struct midi1_serial_data *)dev->data)->running_status_tx = 0;
			k_mutex_unlock(&((struct midi1_serial_data *)dev->data)->tx_lock);
		}
		break;
	}
	case UMP_MT_MIDI1_CHANNEL_VOICE: {
		uint8_t status = UMP_MIDI_STATUS(ump);
		uint8_t cmd = UMP_MIDI_COMMAND(ump);
		if (cmd == 0xC || cmd == 0xD) {
			/* 2-byte messages */
			midi1_serial_tx_enqueue(dev, status);
			midi1_serial_tx_enqueue(dev, UMP_MIDI1_P1(ump));
		} else {
			/* 3-byte messages */
			midi1_serial_tx_enqueue(dev, status);
			midi1_serial_tx_enqueue(dev, UMP_MIDI1_P1(ump));
			midi1_serial_tx_enqueue(dev, UMP_MIDI1_P2(ump));
		}
		break;
	}
	case UMP_MT_DATA_64: {
		/* SysEx7 */
		uint8_t status = (ump.data[0] >> 20) & 0x0F;
		uint8_t count = (ump.data[0] >> 16) & 0x0F;
		uint8_t data[6];

		data[0] = (ump.data[0] >> 8) & 0x7F;
		data[1] = ump.data[0] & 0x7F;
		data[2] = (ump.data[1] >> 24) & 0x7F;
		data[3] = (ump.data[1] >> 16) & 0x7F;
		data[4] = (ump.data[1] >> 8) & 0x7F;
		data[5] = ump.data[1] & 0x7F;

		if (status == UMP_SYSEX7_STATUS_START || status == UMP_SYSEX7_STATUS_COMPLETE) {
			midi1_sysex_start(dev);
		}

		midi1_sysex_data_bulk(dev, data, count);

		if (status == UMP_SYSEX7_STATUS_END || status == UMP_SYSEX7_STATUS_COMPLETE) {
			midi1_sysex_stop(dev);
		}
		break;
	}
	default:
		break;
	}
}

static const struct midi1_serial_api midi1_serial_driver_api = {
	.register_callbacks = midi1_serial_register_callbacks,
	.receiveparser = midi1_serial_receiveparser,
	.receiveparser_ump = midi1_serial_receiveparser_ump,
	.note_on = midi1_serial_note_on,
	.note_off = midi1_serial_note_off,
	.control_change = midi1_serial_control_change,
	.channelaftertouch = midi1_serial_channelaftertouch,
	.program_change = midi1_serial_program_change,
	.polyaftertouch = midi1_serial_polyaftertouch,
	.modwheel = midi1_serial_modwheel,
	.pitchwheel = midi1_serial_pitchwheel,
	.mtc_quarter_frame = midi1_serial_mtc_quarter_frame,
	.song_position = midi1_serial_song_position,
	.song_select = midi1_serial_song_select,
	.tune_request = midi1_serial_tune_request,
	.timingclock = midi1_serial_timingclock,
	.start = midi1_serial_start,
	.continu = midi1_serial_continue,
	.stop = midi1_serial_stop,
	.active_sensing = midi1_serial_active_sensing,
	.reset = midi1_serial_reset,
	.sysex_start = midi1_sysex_start,
	.sysex_char = midi1_sysex_char,
	.sysex_data_bulk = midi1_sysex_data_bulk,
	.sysex_stop = midi1_sysex_stop};

#define DT_DRV_COMPAT midi1_serial
#define MIDI1_SERIAL_DEFINE(inst)                                                                  \
	static struct midi1_serial_data midi1_serial_data_##inst;                                  \
	static const struct midi1_serial_config midi1_serial_config_##inst = {                     \
		.uart = DEVICE_DT_GET(DT_INST_PROP(inst, uart)),                                   \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(inst, midi1_serial_init, NULL, &midi1_serial_data_##inst,            \
			      &midi1_serial_config_##inst, POST_KERNEL, 79,                        \
			      &midi1_serial_driver_api);
DT_INST_FOREACH_STATUS_OKAY(MIDI1_SERIAL_DEFINE)
