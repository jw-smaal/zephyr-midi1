.. _midi1_arp_phase_shift_sample:

MIDI 1.0 Phasing Arpeggiator Sample
###################################

Overview
********
This sample application implements a minimalist-inspired phasing arpeggiator using a clean, modular embedded design. 
It features evolving rhythmic patterns and independent layer drift.

Key Features:
* **Modular Architecture:** Arpeggiator logic is fully encapsulated in ``arp.c/h``, ensuring a clean separation between musical state and hardware drivers.
* **Slow Phasing Drift:** An optional mode where the upper octave layer lags behind the master clock by one MIDI pulse every 8 notes, creating a slowly shifting rhythmic phase.
* **Additive/Subtractive Process Mode:** An evolving sequence mode that builds up notes one-by-one and then tears them down, creating complex and changing harmonic structures.
* **Mode Selector:** Use the onboard **SW3 button** to cycle through four arpeggiator modes:
    1. **SYNC:** Steady counter-point.
    2. **PHASE:** Steady sequence with rhythmic drift.
    3. **PROCESS:** Evolving sequence without drift.
    4. **PH+PROC:** Evolving sequence with rhythmic drift.
* **Condensed Logging:** Provides a clean, single-line console overview of the arpeggiator state and chord transpositions.

Requirements
************
* A board with UART configured for MIDI, a hardware counter, and user-accessible LEDs/Buttons (like ``frdm_mcxc242``).

Building and Running
********************
Build the sample using west:

.. code-block:: bash

   west build -b frdm_mcxc242 modules/lib/midi1/samples/midi1_arp_phase_shift

Once running, use SW2 to toggle Latch and SW3 to cycle through the phasing and process modes. Watch the console for a live visualization of the evolving arpeggio.
