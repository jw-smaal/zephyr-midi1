.. _midi1_clock_generator_sample:

MIDI 1.0 Clock Generator Sample
###############################

:License: Apache-2.0
:Author: Jan-Willem Smaal <usenet@gispen.org>
:Copyright: Jan-Willem Smaal <usenet@gispen.org>

A hardware-based MIDI 1.0 timing clock (24 PPQN) generator for Zephyr RTOS.

Overview
********

This sample demonstrates how to use the ``midi1_clock_cntr`` driver to generate a stable MIDI timing clock at a user-defined BPM. The driver leverages the Zephyr counter API to provide accurate timing pulses.

Features
********

- Generates 24 Pulses Per Quarter Note (PPQN).
- Scaled BPM (SBPM) support for fractional tempo resolution (e.g., 120.00 BPM).
- Uses ``sbpm_to_str()`` for human-readable tempo output.

Hardware Requirements
*********************

Requires a board with a counter device (timer) compatible with the Zephyr counter API.
Examples:

- **NXP FRDM-K64F** (PIT timer)
- **NXP FRDM-MCXC242** (LPTIMER/PIT)

Building
********

From your Zephyr workspace:

.. code-block:: bash

   west build -b <your_board> modules/lib/midi1/samples/midi1_clock_generator

Running
*******

Once flashed, the sample will initialize the clock generator and start outputting timing pulses at 120.00 BPM. The console will print the status every 5 seconds.

License
*******

Distributed under the Apache License, Version 2.0.
