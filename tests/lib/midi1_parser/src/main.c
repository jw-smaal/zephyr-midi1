/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/midi/midi1.h>
#include <zephyr/drivers/midi/midi1_serial.h>

/*
 * Test globals
 */
struct midi1_serial_data test_data;
const struct device test_dev = {
	.data = &test_data,
};

static uint32_t note_on_count;
static uint32_t note_off_count;
static uint32_t realtime_count;
static uint32_t sysex_start_count;
static uint32_t sysex_data_count;
static uint32_t sysex_stop_count;
static uint8_t last_note;
static uint8_t last_velocity;
static uint8_t last_realtime;

/*
 * Callbacks
 */
static uint32_t song_pos_count;
static uint16_t last_song_pos;
static uint32_t program_change_count;
static uint32_t channel_aftertouch_count;
static uint32_t mtc_count;
static uint32_t tune_request_count;

static void test_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
	note_on_count++;
	last_note = note;
	last_velocity = velocity;
}

static void test_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
	note_off_count++;
}

static void test_song_position(uint16_t pos)
{
	song_pos_count++;
	last_song_pos = pos;
}

static void test_program_change(uint8_t channel, uint8_t number)
{
	program_change_count++;
}

static void test_channel_aftertouch(uint8_t channel, uint8_t pressure)
{
	channel_aftertouch_count++;
}

static void test_mtc_quarter_frame(uint8_t data)
{
	mtc_count++;
}

static void test_tune_request(void)
{
	tune_request_count++;
}

static void test_realtime(uint8_t msg)
{
	realtime_count++;
	last_realtime = msg;
}

static void test_sysex_start(void)
{
	sysex_start_count++;
}

static void test_sysex_data(uint8_t data)
{
	sysex_data_count++;
}

static void test_sysex_stop(void)
{
	sysex_stop_count++;
}

static void noop_u8(uint8_t val)
{
}

static void reset_counters(void)
{
	note_on_count = 0;
	note_off_count = 0;
	realtime_count = 0;
	sysex_start_count = 0;
	sysex_data_count = 0;
	sysex_stop_count = 0;
	song_pos_count = 0;
	program_change_count = 0;
	channel_aftertouch_count = 0;
	mtc_count = 0;
	tune_request_count = 0;
	memset(&test_data, 0, sizeof(test_data));
	k_msgq_init(&test_data.msgq, test_data.msgq_buffer, MSG_SIZE, MSGQ_SIZE);
	test_data.cb.note_on = test_note_on;
	test_data.cb.note_off = test_note_off;
	test_data.cb.realtime = test_realtime;
	test_data.cb.sysex_start = test_sysex_start;
	test_data.cb.sysex_data = test_sysex_data;
	test_data.cb.sysex_stop = test_sysex_stop;
	test_data.cb.song_position = test_song_position;
	test_data.cb.song_select = noop_u8;
	test_data.cb.program_change = test_program_change;
	test_data.cb.channel_aftertouch = test_channel_aftertouch;
	test_data.cb.mtc_quarter_frame = test_mtc_quarter_frame;
	test_data.cb.tune_request = test_tune_request;
}

static void inject_byte(uint8_t b)
{
	zassert_equal(k_msgq_put(&test_data.msgq, &b, K_NO_WAIT), 0, "Msgq full");
}

static void run_parser(uint32_t count)
{
	for (uint32_t i = 0; i < count; i++) {
		midi1_serial_receiveparser(&test_dev);
	}
}

/**
 * @brief Test basic Note On message
 */
ZTEST(midi1_parser, test_basic_note_on)
{
	reset_counters();
	inject_byte(0x90);
	inject_byte(0x3C);
	inject_byte(0x40);
	run_parser(3);

	zassert_equal(note_on_count, 1, "Should have 1 Note On");
	zassert_equal(last_note, 0x3C, "Wrong note");
	zassert_equal(last_velocity, 0x40, "Wrong velocity");
}

/**
 * @brief Test Note On with Velocity 0 (MIDI 1.0 Spec rule)
 * Velocity 0 for Note On MUST be treated as Note Off.
 */
ZTEST(midi1_parser, test_note_on_velocity_zero)
{
	reset_counters();
	inject_byte(0x90);
	inject_byte(0x3C);
	inject_byte(0x00);
	run_parser(3);

	zassert_equal(note_on_count, 0, "Should NOT have Note On");
	zassert_equal(note_off_count, 1, "Should have 1 Note Off (Velocity 0 rule)");
}

/**
 * @brief Test Running Status (MIDI 1.0 Spec rule)
 */
ZTEST(midi1_parser, test_running_status)
{
	reset_counters();
	/* First Note On with Status */
	inject_byte(0x90);
	inject_byte(0x3C);
	inject_byte(0x40);
	/* Second Note On without Status (Running Status) */
	inject_byte(0x3E);
	inject_byte(0x7F);
	run_parser(5);

	zassert_equal(note_on_count, 2, "Should have 2 Note Ons");
}

/**
 * @brief Test Real-time message interleaved (MIDI 1.0 Spec rule)
 * Real-time messages can occur anywhere and do NOT affect running status.
 * This test puts RT message between status and data, and between data bytes.
 */
ZTEST(midi1_parser, test_interleaved_realtime_complex)
{
	reset_counters();
	/* 0x90 (Status), 0xF8 (RT), 0x3C (Note), 0xFE (RT), 0x40 (Velocity) */
	inject_byte(0x90);
	inject_byte(0xF8);
	inject_byte(0x3C);
	inject_byte(0xFE);
	inject_byte(0x40);
	run_parser(5);

	zassert_equal(note_on_count, 1, "Should have 1 Note On");
	zassert_equal(realtime_count, 2, "Should have 2 Real-time messages");
}

/**
 * @brief Test SysEx with interleaved Real-time (MIDI 1.0 Spec rule)
 */
ZTEST(midi1_parser, test_sysex_realtime)
{
	reset_counters();
	/* F0 (Start), 01, F8 (Clock), 02, F7 (Stop) */
	inject_byte(0xF0);
	inject_byte(0x01);
	inject_byte(0xF8);
	inject_byte(0x02);
	inject_byte(0xF7);
	run_parser(5);

	zassert_equal(sysex_start_count, 1, "SysEx Start");
	zassert_equal(sysex_data_count, 2, "SysEx Data");
	zassert_equal(sysex_stop_count, 1, "SysEx Stop");
	zassert_equal(realtime_count, 1, "Real-time should be parsed");
}

/**
 * @brief Test SysEx terminated by other Status (MIDI 1.0 Spec rule)
 * Any status byte other than Real-time terminates a SysEx message.
 */
ZTEST(midi1_parser, test_sysex_terminated_by_status)
{
	reset_counters();
	/* F0 (Start), 01, 02, 0x90 (Note On - TERMINATES SysEx) */
	inject_byte(0xF0);
	inject_byte(0x01);
	inject_byte(0x02);
	inject_byte(0x90);
	inject_byte(0x3C);
	inject_byte(0x40);
	run_parser(6);

	zassert_equal(sysex_start_count, 1, "SysEx Start");
	zassert_equal(sysex_data_count, 2, "SysEx Data");
	zassert_equal(sysex_stop_count, 0, "SysEx Stop (EOX) should not be called");
	zassert_equal(note_on_count, 1,
		      "Note On should be parsed immediately after SysEx termination");
}

/**
 * @brief Test Song Position Pointer (14-bit value)
 */
ZTEST(midi1_parser, test_song_position_pointer)
{
	reset_counters();
	/* F2 (Song Pos), LSB, MSB */
	/* 14-bit value: 0x1234 -> LSB: 0x34, MSB: 0x24 (since only 7 bits used) */
	inject_byte(0xF2);
	inject_byte(0x34);
	inject_byte(0x24);
	run_parser(3);

	zassert_equal(song_pos_count, 1, "Should have 1 Song Position message");
	zassert_equal(last_song_pos, (0x24 << 7) | 0x34, "Wrong 14-bit value reconstruction");
}

/**
 * @brief Test Running Status Reset (MIDI 1.0 Spec rule)
 * System Common messages (except Real-time) should reset running status.
 */
ZTEST(midi1_parser, test_running_status_reset)
{
	reset_counters();
	/* 0x90, 0x3C, 0x40 -> Sets running status 0x90 */
	inject_byte(0x90);
	inject_byte(0x3C);
	inject_byte(0x40);
	/* 0xF3 (Song Select) -> Resets running status */
	inject_byte(0xF3);
	inject_byte(0x01);
	/* 0x3E, 0x7F -> Without status byte, this should be ignored or fail */
	inject_byte(0x3E);
	inject_byte(0x7F);
	run_parser(7);

	zassert_equal(note_on_count, 1, "Only first note should be parsed");
}

/**
 * @brief Test Status Byte Override
 * If a status byte is received when a data byte was expected, it resets the parser.
 */
ZTEST(midi1_parser, test_status_override)
{
	reset_counters();
	/* 0x90 (Note On), 0x3C (Note), 0x90 (New Status - OVERRIDE), 0x3E, 0x40 */
	inject_byte(0x90);
	inject_byte(0x3C);
	inject_byte(0x90);
	inject_byte(0x3E);
	inject_byte(0x40);
	run_parser(5);

	zassert_equal(note_on_count, 1, "Should have 1 complete Note On");
	zassert_equal(last_note, 0x3E, "Should be the second note");
}

/**
 * @brief Test Program Change Running Status (1-byte data messages)
 */
ZTEST(midi1_parser, test_program_change_running_status)
{
	reset_counters();
	/* 0xC0 (PC), 0x05 (Data) */
	inject_byte(0xC0);
	inject_byte(0x05);
	/* Running Status: 0x07 (Data only) */
	inject_byte(0x07);
	run_parser(3);

	zassert_equal(program_change_count, 2, "Should have 2 Program Changes");
}

/**
 * @brief Test MTC Quarter Frame (2-byte System Common)
 */
ZTEST(midi1_parser, test_mtc_quarter_frame)
{
	reset_counters();
	/* F1 (MTC), 0x01 (Data) */
	inject_byte(0xF1);
	inject_byte(0x01);
	run_parser(2);

	zassert_equal(mtc_count, 1, "Should have 1 MTC Quarter Frame");
}

/**
 * @brief Test Tune Request (1-byte System Common)
 */
ZTEST(midi1_parser, test_tune_request)
{
	reset_counters();
	/* F6 (Tune Request) - No data bytes */
	inject_byte(0xF6);
	run_parser(1);

	zassert_equal(tune_request_count, 1, "Should have 1 Tune Request");
}

/**
 * @brief Test the "Real-time Storm"
 * Real-time messages interleaved between EVERY byte of a 3-byte message.
 */
ZTEST(midi1_parser, test_realtime_storm)
{
	reset_counters();
	/* 0x90 (Status), F8 (RT), 3C (Note), FE (RT), 40 (Velocity), F8 (RT) */
	inject_byte(0x90);
	inject_byte(0xF8);
	inject_byte(0x3C);
	inject_byte(0xFE);
	inject_byte(0x40);
	inject_byte(0xF8);
	run_parser(6);

	zassert_equal(note_on_count, 1, "Should parse Note On despite RT storm");
	zassert_equal(realtime_count, 3, "Should parse 3 RT messages");
}

/**
 * @brief Test SysEx recovery after malformed stream
 */
ZTEST(midi1_parser, test_sysex_recovery)
{
	reset_counters();
	/* Start SysEx, some data, then a Note On status (terminates SysEx) */
	inject_byte(0xF0);
	inject_byte(0x01);
	inject_byte(0x02);
	inject_byte(0x91); /* Note On Ch2 */
	inject_byte(0x3C);
	inject_byte(0x40);
	run_parser(6);

	zassert_equal(sysex_start_count, 1, "Should start SysEx");
	zassert_equal(sysex_data_count, 2, "Should have 2 bytes of SysEx data");
	zassert_equal(note_on_count, 1, "Should recover and parse Note On immediately");
}

/*
 * Mock for UART to test TX logic
 */

static uint8_t tx_buf[256];
static uint32_t tx_ptr;

static void mock_uart_poll_out(const struct device *dev, unsigned char out_char)
{
	tx_buf[tx_ptr++] = out_char;
}

static struct uart_driver_api mock_uart_api = {
	.poll_out = mock_uart_poll_out,
};

static const struct device mock_uart = {
	.api = &mock_uart_api,
};

/**
 * @brief Test the 15-message status insertion rule
 */
ZTEST(midi1_parser, test_tx_running_status_reinsertion)
{
	struct midi1_serial_data data;
	struct midi1_serial_config cfg = {.uart = &mock_uart};
	const struct device dev = {
		.data = &data,
		.config = &cfg,
	};

	memset(&data, 0, sizeof(data));
	k_mutex_init(&data.tx_lock);
	tx_ptr = 0;

	/* Send 16 Note Ons.
	 * Rule: After CONFIG_MIDI1_SERIAL_RUNNING_STATUS_REPEAT (15), re-insert status.
	 */
	for (int i = 0; i < 16; i++) {
		midi1_serial_note_on(&dev, 0, 60, 100);
	}

	/* Each Note On is 2 bytes with running status, 3 bytes with status.
	 * 1st: [0x90, 60, 100] (3 bytes)
	 * 2..15: [60, 100] (2 bytes each * 14 = 28 bytes)
	 * 16th: [0x90, 60, 100] (3 bytes) - This is where re-insertion happens
	 * Total expected: 3 + 28 + 3 = 34 bytes.
	 * If no re-insertion: 3 + 30 = 33 bytes.
	 */
	zassert_equal(tx_ptr, 34, "Expected 34 bytes with status re-insertion at 16th message");
	zassert_equal(tx_buf[0], 0x90, "First byte should be status");
	zassert_equal(tx_buf[31], 0x90,
		      "32nd byte should be re-inserted status (3 + 14*2 = 31 index)");
}

ZTEST_SUITE(midi1_parser, NULL, NULL, NULL, NULL, NULL);
