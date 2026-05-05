.. _chord_display_sample:

MIDI Chord Display
##################

Overview
********

This sample demonstrates a thread-safe Zephyr application that detects MIDI chords in real-time and displays them on an LVGL-powered LCD screen. The application leverages the ``midi1_harmonic`` library to analyze incoming MIDI Note messages and determine the current chord, handling complex chords and inversions.

Hardware Requirements
*********************

To run this application, the following hardware is required:

* **NXP FRDM-RW612** development board (``frdm_rw612``).
* **LCD PAR S035 SPI** Shield (or a compatible LVGL-supported SPI display).
* A **MIDI 1.0 Serial interface** (UART) connected to the board to receive MIDI data.
* A MIDI controller or sequencer sending Note messages.

Software Dependencies
*********************

This application depends on the following modules and libraries:

* **midi1_serial**: Required for handling the low-level MIDI 1.0 UART communication.
* **Zephyr OS**: For RTOS features, threading, and hardware abstraction.
* **LVGL**: For rendering the graphical user interface.

Functional Description
**********************

The application initializes the display using the LVGL library and creates a dedicated thread (``lvgl_thread``) to manage UI updates safely. As MIDI Note On and Note Off messages arrive via the ``midi1_serial`` interface, they are parsed and analyzed by the chord detection logic. 

When the active notes change, the application updates a shared state and signals the display thread to render the new chord name prominently on the screen. The text is configured to wrap across multiple lines and center automatically to accommodate long chord names.

Building and Running
********************

Build the application for the ``frdm_rw612`` board with the LCD shield:

.. code-block:: bash

   ./scripts/compile_rw612.sh

*(This script automatically includes the necessary board and shield configurations for west build).*
