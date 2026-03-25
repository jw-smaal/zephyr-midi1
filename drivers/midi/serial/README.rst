MIDI Serial Driver for Zephyr RTOS
################################

This driver implements the MIDI 1.0 protocol over a serial UART interface, featuring a hybrid transmit architecture optimized for both low jitter (MIDI Clock) and high throughput (Note/SysEx data).

Overview
********

The ``midi1_serial`` driver is designed to handle the strict timing requirements of MIDI while preventing thread blocking during heavy data transmission. It supports:

- **Interrupt-Driven TX:** For standard messages (Notes, Control Change, SysEx).
- **Zero-Jitter Polling:** For Real-Time messages (MIDI Clock, Start, Stop).
- **Running Status:** Both Transmit (TX) and Receive (RX) support to optimize bandwidth.
- **Concurrency Safety:** Protects UART hardware access when mixing ISR and Thread contexts.

Architecture
************

Hybrid Transmit Mechanism
=========================

The driver uses a **Hybrid Transmit Strategy** to balance latency and jitter:

1.  **Standard Messages (Low Priority):**
    Messages like *Note On*, *Control Change*, and *SysEx* are written to a software **Ring Buffer**. The UART interrupt service routine (ISR) then drains this buffer in the background. This ensures that sending a large chord or SysEx dump does not block the calling thread.

2.  **Real-Time Messages (High Priority):**
    Time-critical messages like *Timing Clock (0xF8)*, *Start*, and *Stop* bypass the ring buffer. They are sent immediately using ``uart_poll_out()``, temporarily disabling the UART TX interrupt to ensure exclusive hardware access. This guarantees minimal jitter for synchronization.

Receive Path
============

Incoming bytes are handled by a UART ISR which pushes them into a Zephyr Message Queue (``k_msgq``). A dedicated parser thread or loop then reads from this queue to reconstruct MIDI messages and invoke callbacks.

Configuration
*************

The driver behavior can be tuned via Kconfig:

- ``CONFIG_MIDI1_SERIAL_TX_BUF_SIZE``: Size of the transmit ring buffer (default: 128 bytes). Increase this if you experience dropped messages during dense Note/SysEx bursts.
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

   /* Sends a Note On (Buffered, Non-Blocking) */
   midi1_serial_note_on(midi_dev, 0, 60, 100);

   /* Sends a Clock Tick (Immediate, Zero-Jitter) */
   midi1_serial_timingclock(midi_dev);

Receiving Data
==============

Register callbacks to handle incoming messages:

.. code-block:: c

   struct midi1_serial_callbacks cb = {
       .note_on = my_note_on_handler,
       .control_change = my_cc_handler,
       .realtime = my_realtime_handler
   };

   midi1_serial_register_callbacks(midi_dev, &cb);

   /* Call the parser periodically */
   while (1) {
       midi1_serial_receiveparser(midi_dev);
   }
