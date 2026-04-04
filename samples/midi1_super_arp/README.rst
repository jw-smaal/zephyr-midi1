.. _midi1_super_arp_sample:

MIDI 1.0 Super Arpeggiator Sample
#################################

Overview
********
This sample application extends the basic arpeggiator with advanced sequencing features, allowing for more complex harmonic textures.

Key Features:
* **Multi-Directional Sequencing:** Supports four playback modes:
    * **UP:** Ascends through the notes.
    * **DOWN:** Descends through the notes.
    * **UP_DOWN:** Ping-pongs from the lowest to the highest note and back.
    * **DOWN_UP:** Ping-pongs from the highest to the lowest note and back.
* **Octave Spreading:** Automatically expands the held chord across multiple octaves (configurable via ``ARP_OCTAVES``).
* **Safe Pitch Clamping:** Ensures all transposed notes remain within the valid MIDI range (0-127).
* **Rich Logging:** Provides real-time feedback on the console about the current sequence direction and octave offset.

Requirements
************
* A board with UART configured for MIDI (31250 baud) and a hardware counter (like ``frdm_mcxc242``).

Building and Running
********************
Build the sample using west:

.. code-block:: bash

   west build -b frdm_mcxc242 modules/lib/midi1/samples/midi1_super_arp

Once running, play a chord. The arpeggiator will expand your chord across two octaves and cycle through the notes in an Up-Down pattern by default.
