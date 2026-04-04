.. _midi1_octave_arp_sample:

MIDI 1.0 Converging Arpeggiator Sample
######################################

Overview
********
This sample application implements a highly musical "converging" arpeggiator that utilizes counter-point motion and integrated hardware controls.

Key Features:
* **Counter-point Arpeggiation:** Plays two layers simultaneously for every step:
    1. A base octave note moving in an **UPWARD** sequence.
    2. A mirrored upper-octave note moving in a **REVERSED** sequence.
* **Physical Latch Mode:** Use the onboard **SW2 button** to toggle arpeggiator latching. The **Red LED** indicates when Latch is active.
* **Tempo Heartbeat:** The **Blue LED** flashes in sync with the current MIDI clock (every quarter note), providing clear visual tempo feedback.
* **Intelligent Buffer Management:** Automatically clears and replaces the held sequence when a new chord is played while in Latch mode.

Requirements
************
* A board with UART configured for MIDI, a hardware counter, and user-accessible LEDs/Buttons (like ``frdm_mcxc242``).

Building and Running
********************
Build the sample using west:

.. code-block:: bash

   west build -b frdm_mcxc242 modules/lib/midi1/samples/midi1_octave_arp

Press SW2 to enable Latch (Red LED on). Play a chord and release; the arpeggio will continue playing until you play a new chord.
