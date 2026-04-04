.. _midi1_arp_phase_shift_experimental:

MIDI 1.0 Phasing Arpeggiator (Experimental)
###########################################

:Author: Jan-Willem Smaal <usenet@gispen.org>

Overview
********
This is a Phasing Arpeggiator sample with real-time hardware controls for BPM and a visual dashboard.

Key Features:
* **Modular Architecture:** Logic is separated into ``arp`` (sequencing), ``tempo`` (ADC management), and ``display_mgr`` (OLED interface).
* **ADC BPM Control:** Dynamically adjust the arpeggiator speed from **5.00 to 360.00 BPM** using a 10k Ohm potentiometer connected to pin **A1 (PTE16)**.
* **OLED Dashboard:** Real-time visualization on an **SSD1306** display, showing:
    * Current arpeggiator Mode and Latch status.
    * Live BPM readout from the potentiometer.
    * Current note count in the held chord.
* **Rhythmic Phasing & Process Modes:** 
    * Slow drift logic for minimalist textures.
    * Additive/Subtractive sequence evolution.
* **Low-Latency Design:** The display update runs in a low-priority background thread using state snapshotting to ensure the arpeggiator's MIDI timing remains rock-solid.

Hardware Configuration
**********************
* **FRDM-MCXC242:** 
    * Potentiometer: Pin **A1 (PTE16)**.
    * OLED (SSD1306): I2C1 (Arduino Header SCL/SDA).
    * Button SW2: Latch Toggle.
    * Button SW3: Mode Selector.

Requirements
************
* A board with UART configured for MIDI, a hardware counter, an ADC, and an optional SSD1306 I2C display.

Building and Running
********************
Build the experimental sample for the FRDM-MCXC242:

.. code-block:: bash

   west build -b frdm_mcxc242 WORK-GIT/midi1_arp_phase_shift

Once flashed, use the potentiometer to sweep the BPM and watch the OLED display update in real-time. Use SW2 and SW3 to explore the various phasing and additive musical patterns.
