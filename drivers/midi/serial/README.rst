MIDI Serial Driver for Zephyr RTOS
##################################

This driver implements the MIDI 1.0 protocol over a serial UART interface, featuring a prioritized interrupt-driven transmit architecture optimized for both low jitter (MIDI Clock) and high throughput (Note/SysEx data).

Overview
********

The ``midi1_serial`` driver is designed to handle the strict timing requirements of MIDI while preventing thread blocking during heavy data transmission. It supports:

- **Prioritized Interrupt-Driven TX:** Uses dual ring buffers to ensure Real-Time messages take precedence.
- **Low-Jitter Timing:** Real-Time messages (Clock, Start, Stop) bypass the standard transmit queue.
- **Running Status:** Both Transmit (TX) and Receive (RX) support to optimize bandwidth.
- **Concurrency Safety:** Protects UART hardware access and internal state across ISR and Thread contexts.

Architecture
************

Prioritized Transmit Mechanism
==============================

The driver uses a **Dual-Buffer Transmit Strategy** to balance latency and jitter:

1.  **Standard Messages (Low Priority):**
    Messages like *Note On*, *Control Change*, and *SysEx* are written to a software **Ring Buffer** (``tx_ringbuf``). The UART interrupt service routine (ISR) drains this buffer when no high-priority data is pending.

2.  **Real-Time Messages (High Priority):**
    Time-critical messages like *Timing Clock (0xF8)*, *Start*, and *Stop* are placed in a dedicated **Real-Time Ring Buffer** (``tx_rt_ringbuf``). The ISR always checks this buffer first, ensuring that a Clock tick is never delayed by a large SysEx dump or a dense chord.

Receive Path
============

Incoming bytes are handled by a UART ISR which pushes them into a Zephyr Message Queue (``k_msgq``). A dedicated parser thread or loop then reads from this queue. Note that the parser function is **blocking**, waiting for data via ``k_msgq_get``.

Configuration
*************

The driver behavior can be tuned via Kconfig:

- ``CONFIG_MIDI1_SERIAL_TX_BUF_SIZE``: Size of the standard transmit ring buffer (default: 128 bytes).
- ``CONFIG_MIDI1_SERIAL_ISR_MSGQ_SIZE_MS``: Size of the receive message queue (default: 128 bytes).
- ``CONFIG_MIDI1_SERIAL_RUNNING_STATUS_REPEAT``: How often to refresh the running status byte during transmission (default: 16 messages).
- ``CONFIG_MIDI1_SERIAL_RUNNING_STATUS_TIMEOUT_MS``: Max time before refreshing running status (default: 300 ms).

Observability
*************

The driver maintains an ``overrun_count`` in the ``midi1_serial_data`` structure. This counter increments whenever:

- The RX Message Queue is full (incoming data lost).
- The TX Ring Buffer is full (outgoing data lost).

Application code can monitor this counter to detect system bottlenecks.

API Usage
*********

Initialization
==============

The driver is initialized automatically by the Zephyr kernel if ``status = "okay"`` in the Devicetree.

.. code-block:: c

   const struct device *midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi1_serial));

   if (!device_is_ready(midi_dev)) {
       /* Handle error */
   }

Sending Data
============

.. code-block:: c

   /* Sends a Note On (Buffered, Low Priority) */
   midi1_serial_note_on(midi_dev, CH1, 60, 100);

   /* Sends a Clock Tick (Buffered, High Priority) */
   midi1_serial_timingclock(midi_dev);

Receiving Data
==============

Register callbacks to handle incoming messages. Since the parser is blocking, it is best run in its own thread:

.. code-block:: c

   struct midi1_serial_callbacks cb = {
       .note_on = my_note_on_handler,
       .control_change = my_cc_handler,
       .realtime = my_realtime_handler
   };

   midi1_serial_register_callbacks(midi_dev, &cb);

   /* Blocking parser loop */
   while (1) {
       midi1_serial_receiveparser(midi_dev);
   }
