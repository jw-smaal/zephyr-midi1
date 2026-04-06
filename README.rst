.. _midi1_module:

Zephyr MIDI1 Module
###################

A standalone Zephyr RTOS module providing MIDI 1.0 drivers and protocol helpers.

:Author: Jan-Willem Smaal <usenet@gispen.org>
:Copyright: Jan-Willem Smaal <usenet@gispen.org>
:License: Apache-2.0

Features
********

- **MIDI Serial UART Driver**: Supports MIDI 1.0 over UART with "Running Status" support (transmit and receive) and thread-safe mutex protection for multi-threaded applications.
- **MIDI Clock Generator**: Hardware-based MIDI clock (24 PPQN) generation.
- **MIDI Clock Measurement**: Measures BPM of incoming clock pulses with block averaging.
- **Universal MIDI Packet (UMP)**: Helpers for generating MIDI 1.0 UMP packets.

Integration via West
********************

To set up a fresh Zephyr workspace including this MIDI1.0 module, follow these step-by-step instructions:

1. **Create a workspace directory:**

   Open your terminal and create a folder for your project:

   .. code-block:: bash

      mkdir zephyr-workspace
      cd zephyr-workspace

2. **Initialize and pull all required source code:**

   This command initializes the workspace using this module as the manifest and pulls Zephyr (v4.4.0-rc2), HALs, and this module.

   .. code-block:: bash

      west init -m https://github.com/jw-smaal/zephyr-midi1
      west update

3. **Install Python dependencies:**

   Ensure your environment has all the necessary tools to build Zephyr applications:

   .. code-block:: bash

      pip install -r zephyr/scripts/requirements.txt

Configuration
*************

Enable the desired drivers in your project's ``prj.conf``:

.. code-block:: kconfig

   # Enable MIDI Serial UART Driver
   CONFIG_MIDI1_SERIAL=y

   # Enable MIDI Clock Generator
   CONFIG_MIDI1_CLOCK_CNTR=y

   # Enable MIDI Clock Measurement
   CONFIG_MIDI1_CLOCK_MEAS_CNTR=y

Devicetree Usage
****************

Example for defining a MIDI serial instance in your Devicetree overlay:

.. code-block:: dts

   / {
       midi_serial_0: midi1-serial-0 {
           compatible = "midi1-serial";
           uart = <&uart3>;
           status = "okay";
       };

       aliases {
           i2c0 = &i2c0;
           midi = &uart3;
           counterch0 = &pit0_channel0;
           counterch1 = &pit0_channel1;
       };
   };

   /*
    * Classic MIDI1.0 over DIN5 plug
    */
   &uart3 {
       status = "okay";
       /* Set it to the MIDI baud rate */
       current-speed = <31250>;
   };

   /*
    * Because on the FRDM_K64F each channel has its own IRQ we need to be
    * specific here; we can't just enable pit0 and hope for the best.
    */
   &pit0 {
       status = "okay";
   };

   &pit0_channel0 {
       status = "okay";
   };

   &pit0_channel1 {
       status = "okay";
   };

Basic Usage Examples
********************

Here are some basic examples demonstrating how to use the drivers in your code.

**1. Sending a MIDI Note (Serial Driver)**

.. code-block:: c

   #include <zephyr/device.h>
   #include <zephyr/drivers/midi/midi1_serial.h>

   int main(void) {
       const struct device *midi_dev = DEVICE_DT_GET(DT_NODELABEL(midi_serial_0));
       if (!device_is_ready(midi_dev)) {
           return -ENODEV;
       }
       const struct midi1_serial_api *mid = midi_dev->api;

       /* Send Note On: Channel 1, Note 60 (Middle C), Velocity 100 */
       mid->note_on(midi_dev, CH1, 60, 100);

       /* ... wait ... */

       /* Send Note Off: Channel 1, Note 60, Velocity 0 */
       mid->note_off(midi_dev, CH1, 60, 0);

       return 0;
   }


**2. Generating a MIDI Clock**

.. code-block:: c

   #include <zephyr/device.h>
   #include <zephyr/drivers/midi/midi1_clock_cntr.h>

   int main(void) {
       const struct device *midi_clock = DEVICE_DT_GET_ANY(midi1_clock_cntr);
       if (!device_is_ready(midi_clock)) {
           return -ENODEV;
       }
       const struct midi1_clock_cntr_api *clk = midi_clock->api;

       /* Generate a 24 PPQN MIDI clock at 120.00 BPM */
       uint16_t target_bpm = 12000;
       clk->gen(midi_clock, target_bpm);

       return 0;
   }

License
*******

Distributed under the Apache License, Version 2.0. See the source files for details.
