Hardware Support
################

:Author: Jan-Willem Smaal <usenet@gispen.org>

This directory contains Devicetree overlays for the various boards supported by the Phasing Arpeggiator. Each overlay configures the necessary MIDI UART, Hardware Counters, and User Interface components (Rotary Encoder, OLED, and Buttons).

User Interface Components
*************************

The Phasing Arpeggiator uses a standardized set of hardware controls across all supported boards.

Rotary Encoder & OLED
=====================

* **Rotary Encoder (GPIO-QDEC):**
    * **Phase A:** Mapped to **D6**.
    * **Phase B:** Mapped to **D7**.
    * **Function:** Adjusts BPM (Coarse/Fine), Phase Drift, or Arpeggiator Mode.
* **Encoder Button:**
    * **Pin:** Mapped to **D2**.
    * **Function:** Cycles through the parameter menu (BPM -> Drift -> Mode). Confirms pending mode changes.
* **OLED Display (SSD1306):**
    * **Interface:** I2C (Arduino Header).
    * **Function:** Provides a real-time dashboard for BPM, mode, drift, and active notes.

Tactile Buttons
===============

* **BPM SHIFT Button:**
    * **Pin:** Mapped to **D10**.
    * **Function:** Hold while turning the encoder for **Fine BPM adjustment** (0.01 BPM increments).
* **Latch Button (SW2):**
    * **Function:** Toggles Arpeggiator Latch mode.
* **Clear Button (SW3):**
    * **Function:** Clears the current arpeggio sequence.

Visual Feedback (LEDs)
======================

* **Red LED (LED0):** Illuminates when **Latch** is active.
* **Blue LED (LED2):** **Tempo Heartbeat** - flashes on every quarter note.
* **Green LED (LED1):** Flashes briefly when the arpeggio is cleared.

Supported Boards
****************

+-----------------------+------------------+-----------------------------+
| Board                 | Counter Device   | MIDI UART                   |
+=======================+==================+=============================+
| **FRDM-MCXC242**      | ``PIT0_CH0``     | ``LPUART1`` (MIKRObus)      |
+-----------------------+------------------+-----------------------------+
| **FRDM-K64F**         | ``PIT0_CH0``     | ``UART3``                   |
+-----------------------+------------------+-----------------------------+
| **FRDM-MCXA156**      | ``CTIMER0``      | ``LPUART1`` (MIKRObus)      |
+-----------------------+------------------+-----------------------------+
| **FRDM-MCXC444**      | ``PIT0_CH0``     | ``LPUART0``                 |
+-----------------------+------------------+-----------------------------+
| **FRDM-MCXW71**       | ``LPTMR0``       | ``LPUART0``                 |
+-----------------------+------------------+-----------------------------+
| **FRDM-RW612**        | ``CTIMER0``      | ``FLEXCOMM3``               |
+-----------------------+------------------+-----------------------------+
| **nRF5340 DK**        | ``TIMER0``       | ``UART1``                   |
+-----------------------+------------------+-----------------------------+
| **Nucleo F030R8**     | ``TIMERS1``      | ``USART1``                  |
+-----------------------+------------------+-----------------------------+

.. note::
   The MIDI serial driver is configured for the standard MIDI baud rate (31250). Ensure your MIDI hardware interface (optocoupler/current loop) is connected to the appropriate RX/TX pins on the specified UART.
