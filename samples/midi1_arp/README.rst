.. _midi1_arp_sample:

MIDI 1.0 Arpeggiator Sample
###########################

Overview
********
This sample application demonstrates the creation of an Arpeggiator using the MIDI 1.0 drivers.
It combines clock generation with both input and output processing, serving as a comprehensive demonstration of the library's capabilities.

The application generates a MIDI clock using the hardware counter driver. It listens for incoming MIDI notes. When a chord (or multiple notes) is played, it arpeggiates those notes, sending them out sequentially synchronized to the generated MIDI clock.

Requirements
************
* A board with UART configured for MIDI (31250 baud) and a hardware counter (like ``frdm_k64f`` or ``frdm_mcxc242``).
* A MIDI interface to connect to a synthesizer and a MIDI keyboard.

Building and Running
********************
Build the sample using west:

.. code-block:: bash

   west build -b <your_board> samples/midi1_arp

Replace ``<your_board>`` with your specific board.

Once flashed, connect your MIDI keyboard to the MIDI IN, and a synthesizer to MIDI OUT. Play a chord on the keyboard to hear it arpeggiated on the synthesizer.
