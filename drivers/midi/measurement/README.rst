MIDI Clock Measurement Driver
#############################

This driver measures the tempo (BPM) of an incoming MIDI 1.0 timing clock (24 PPQN) signal. It uses a hardware counter for high-precision timestamping and **block averaging** for jitter reduction.

Overview
********

The ``midi1_clock_meas_cntr`` driver works by calculating the time intervals between incoming 0xF8 (Timing Clock) pulses. It outputs the result in **Scaled BPM (SBPM)** form (e.g., 12000 for 120.00 BPM).

Features
********

- **Microsecond Precision:** Uses a free-running hardware counter for sub-millisecond accuracy.
- **Jitter-Proof Averaging:** Implements a configurable moving average to stabilize BPM readings even with imperfect sources.
- **Scaled BPM (SBPM):** High-resolution tempo output without floating-point overhead.
- **Valid Bit Indication:** Built-in mechanism to signal when enough pulses have arrived for a stable reading.

Devicetree Configuration
************************

Link the driver to a hardware counter device.

.. code-block:: dts

   / {
       midi_meas: midi1-clock-meas-cntr {
           compatible = "midi1_clock_meas_cntr";
           counter = <&pit0_channel1>;
           status = "okay";
       };
   };

   &pit0 {
       status = "okay";
   };

   &pit0_channel1 {
       status = "okay";
   };

API Usage
*********

Informing the Driver of a Pulse
===============================

The driver depends on your MIDI parser to pulse it whenever a Real-Time Clock byte is received.

.. code-block:: c

   void my_realtime_handler(uint8_t msg) {
       if (msg == RT_TIMING_CLOCK) {
           midi1_clock_meas_cntr_pulse(meas_dev);
       }
   }

Reading the BPM
===============

.. code-block:: c

   if (midi1_clock_meas_cntr_is_valid(meas_dev)) {
       uint16_t sbpm = midi1_clock_meas_cntr_get_sbpm(meas_dev);
       /* result 12000 -> 120.00 BPM */
   }

Configuration
*************

The driver performance can be tuned via Kconfig:

- ``CONFIG_MIDI1_CLOCK_MEAS_CNTR``: Enables the driver.
- ``CONFIG_MIDI1_CLOCK_MEAS_BLOCKAVG_SIZE``: The number of samples to average for BPM calculation (default: 24). Larger values provide more stability but increase latency to tempo changes.
