.. _midi1_joystick:

MIDI 1.0 Joystick Sample
########################

Overview
********

This sample application demonstrates how to use the Zephyr USB Host stack to
connect a USB HID Joystick and translate its movements into MIDI 1.0 messages.

The application performs the following:
- Acts as a USB HID Host.
- Enumerates connected USB devices and identifies HID Class devices.
- Specifically looks for an Interrupt IN endpoint to receive joystick reports.
- Maps the Joystick X-axis to MIDI Pitchwheel changes.
- Maps the Joystick Y-axis to MIDI Modulation Wheel (CC1) changes.
- Sends the generated MIDI messages over a serial UART using the ``midi1_serial`` driver.

Requirements
************

* A board with USB Host support (e.g., FRDM-MCXN947).
* A USB HID compatible joystick.
* A MIDI receiver connected to the designated UART pins.

Building and Running
********************

Build the sample for the FRDM-MCXN947 board:

.. code-block:: console

   west build -b frdm_mcxn947/mcxn947/cpu0 WORK-GIT/zephyr-midi1/samples/midi1_joystick

Flash the board and connect a USB joystick to the USB port. Observe the log
output for joystick coordinates and MIDI message generation.

Sample Output
*************

.. code-block:: console

   *** Booting Zephyr OS build v4.4.99 ***
   [00:00:00.000,000] <inf> main: Starting USB HID Joystick Host sample
   [00:00:05.123,456] <inf> main: Probing HID interface 0
   [00:00:05.123,789] <inf> main: Found Interrupt IN endpoint 0x81, MPS 8
   [00:00:06.000,000] <inf> main: Joystick: X=128, Y=128
   [00:00:06.100,000] <inf> main: Joystick: X=130, Y=128
   [00:00:06.200,000] <inf> main: Joystick: X=130, Y=135
