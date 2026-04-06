.. _midi1_arp_phase_shift_experimental:

MIDI 1.0 Phasing Arpeggiator (Experimental)
###########################################

:Author: Jan-Willem Smaal <usenet@gispen.org>

Overview
********
This is a Phasing Arpeggiator sample with real-time hardware controls for BPM and a visual dashboard.

Key Features:
* **Modular Architecture:** Logic is separated into ``arp`` (sequencing), ``tempo`` (digital clock management), and ``display_mgr`` (OLED interface).
* **Rotary Encoder UI:** Precision control using a 5-pin rotary encoder:
    * **Rotation:** Adjusts the active parameter (indicated by a ``*``).
    * **Click (D2):** Cycles focus between **BPM**, **Drift**, and **Mode**.
    * **Pending Mode Selection:** When adjusting the Arpeggiator Mode, the change is only applied after clicking the encoder button (a ``?`` indicates confirmation is needed).
* **Precision BPM Control:** 
    * **Coarse:** Adjust BPM in **1.00** increments by turning the knob.
    * **Fine (SHIFT):** Hold **SW2** while turning to adjust in **0.01** increments.
* **OLED Dashboard:** Real-time visualization on an **SSD1306** display, showing:
    * Arpeggiator Mode and Latch status.
    * High-precision BPM readout.
    * Phase Drift Cycle value.
    * Active note count.
* **Rhythmic Phasing & Process Modes:** 
    * Slow drift logic for minimalist textures.
    * Additive/Subtractive sequence evolution.

Hardware Configuration
**********************
* **FRDM-MCXC242 (and similar boards):** 
    * **Encoder Phase A:** Pin **D6**.
    * **Encoder Phase B:** Pin **D7**.
    * **Encoder Button:** Pin **D2**.
    * **Button SW2:** SHIFT / Latch Toggle.
    * **Button SW3:** Clear Arpeggiator.
    * **OLED (SSD1306):** Standard Arduino Header I2C pins.

Requirements
************
* A board with UART configured for MIDI, a hardware counter, an ADC, and an optional SSD1306 I2C display.

Building and Running
********************
Build the experimental sample for the FRDM-MCXC242:

.. code-block:: bash

   west build -b frdm_mcxc242 WORK-GIT/midi1_arp_phase_shift

Once flashed, use the potentiometer to sweep the BPM and watch the OLED display update in real-time. Use SW2 and SW3 to explore the various phasing and additive musical patterns.
