.. _harmonic_midi_reactive:

MIDI Reactive Harmonic Sample
##############################

Overview
********

This sample demonstrates the use of the ``midi1_harmonic`` library to transform incoming MIDI Note messages in real-time. It acts as a harmonic "expander" or "harmonizer": for every note received, it outputs a set of notes defined by a selected harmonic mask (scale or chord).

The selection of the harmonic mask is controlled dynamically via MIDI Control Change messages.

Requirements
************

* A board with a MIDI 1.0 Serial interface (UART).
* A MIDI controller or sequencer sending Note and Control Change messages.

Functional Description
**********************

Incoming Notes
==============

When a **Note On** message is received, the sample identifies the current harmonic mask and transposes it to the received note's pitch (treating it as the root). It then sends out a Note On message for every set bit in the mask.

When a **Note Off** message is received, it sends corresponding Note Off messages for all notes previously generated in that scale/chord.

Mod Wheel Control (CC 1)
========================

The **Modulation Wheel** (Control Change 1) is used to switch between different scales and chords. The 0-127 range is partitioned into 16 distinct modes:

+-----------+-----------------------+------------+
| CC Value  | Harmonic Mode          | Type       |
+===========+=======================+============+
| 0 - 8     | Major Scale           | Scale      |
+-----------+-----------------------+------------+
| 9 - 16    | Natural Minor Scale   | Scale      |
+-----------+-----------------------+------------+
| 17 - 24   | Harmonic Minor Scale  | Scale      |
+-----------+-----------------------+------------+
| 25 - 32   | Dorian Scale          | Scale      |
+-----------+-----------------------+------------+
| 33 - 40   | Phrygian Scale        | Scale      |
+-----------+-----------------------+------------+
| 41 - 48   | Lydian Scale          | Scale      |
+-----------+-----------------------+------------+
| 49 - 56   | Mixolydian Scale      | Scale      |
+-----------+-----------------------+------------+
| 57 - 64   | Blues Scale           | Scale      |
+-----------+-----------------------+------------+
| 65 - 72   | Major 7th Chord       | Chord      |
+-----------+-----------------------+------------+
| 73 - 80   | Minor 7th Chord       | Chord      |
+-----------+-----------------------+------------+
| 81 - 88   | Dominant 7th Chord    | Chord      |
+-----------+-----------------------+------------+
| 89 - 96   | Minor-Major 7th Chord | Chord      |
+-----------+-----------------------+------------+
| 97 - 104  | Diminished 7th Chord  | Chord      |
+-----------+-----------------------+------------+
| 105 - 112 | Sus4 Chord            | Chord      |
+-----------+-----------------------+------------+
| 113 - 120 | Whole Tone Scale      | Scale      |
+-----------+-----------------------+------------+
| 121 - 127 | Chromatic Scale       | Scale      |
+-----------+-----------------------+------------+

Building and Running
********************

Build the sample for your board (e.g., ``frdm_mcxc242``):

.. code-block:: bash

   west build -b frdm_mcxc242 WORK-GIT/zephyr-midi1/samples/harmonic_midi_reactive

Sample Output
*************

When running, the sample logs the current mode to the console whenever the Mod Wheel is moved:

.. code-block:: console

   [00:00:05.123] <inf> midi_reactive: Mode: MAJOR SCALE
   [00:00:08.456] <inf> midi_reactive: Mode: DOMINANT 7TH CHORD
   [00:00:10.789] <inf> midi_reactive: Note On: 60 (vel 100), Wheel: 85
