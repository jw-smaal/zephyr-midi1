.. _midi1_samples:

MIDI 1.0 Samples
################

This directory contains sample applications demonstrating the use of the Zephyr MIDI 1.0 module.

Available Samples
*****************

- **midi1_serial**: Basic MIDI serial UART transmit/receive example.
- **midi1_clock_generator**: Demonstrates how to generate MIDI timing clock pulses (24 PPQN) at a specific BPM.
- **midi1_clock_measurement**: Shows how to measure the BPM of an incoming MIDI clock signal.

How to build
************

To build any of these samples, use the following ``west`` command from your Zephyr workspace:

.. code-block:: bash

   west build -b <your_board> modules/lib/midi1/samples/<sample_name>

Replace ``<your_board>`` with your target board (e.g., ``frdm_k64f``) and ``<sample_name>`` with the desired sample directory.
