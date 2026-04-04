.. _midi1_arp_sample:

MIDI 1.0 Arpeggiator Sample
###########################

Overview
********
This sample application demonstrates a baseline arpeggiator implementation using the MIDI 1.0 drivers. 
It serves as a simple, elegant starting point for MIDI sequence processing in Zephyr RTOS.

Key Features:
* **Internal Clock Generation:** Uses the hardware counter driver to generate a rock-solid MIDI clock.
* **Velocity Capture:** Accurately records the velocity of each incoming note and preserves the dynamics in the arpeggiated playback.
* **16th Note Arpeggiation:** Triggers steps every 6 MIDI pulses (based on standard 24 PPQN).
* **Robust Note Handling:** Includes logic to handle MIDI "Note On with Velocity 0" as Note Off for compatibility with various controllers.
* **Thread Safety:** Uses mutexes to safely share note buffers between the MIDI receive thread and the system workqueue.

Requirements
************
* A board with UART configured for MIDI (31250 baud) and a hardware counter (like ``frdm_mcxc242``).
* A MIDI interface PCB (optocoupler for input, current loop for output).

Building and Running
********************
Build the sample using west:

.. code-block:: bash

   west build -b frdm_mcxc242 modules/lib/midi1/samples/midi1_arp

Flash and connect your MIDI hardware. Play a chord to hear it arpeggiated with your original playing dynamics.
