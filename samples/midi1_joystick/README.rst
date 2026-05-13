.. _midi1_joystick_sample:

USB HID Joystick to MIDI Bridge
###############################

Overview
********

This sample application demonstrates how to use the Zephyr USB Host stack to
interface with a USB HID Joystick and translate its axis and button inputs
into MIDI 1.0 messages.

The sample is specifically tuned for the **Thrustmaster T.16000M**, leveraging
its high-resolution 14-bit Hall Effect sensors for precise MIDI Pitchwheel
and Modulation control.

Hardware Mapping
****************

The application maps joystick inputs to MIDI messages as follows:

*   **X-Axis**: MIDI Pitchwheel (14-bit resolution).
*   **Y-Axis (Push Forward)**: MIDI Modulation Wheel (CC 1).
*   **Twist (Yaw)**: MIDI Filter Cutoff (CC 74).
*   **Fader (Throttle)**: MIDI Volume (CC 7).
*   **Fire Button (Trigger)**: MIDI Note 60 (Middle C) On/Off.

Requirements
************

*   A board with USB Host support (e.g., FRDM-RW612).
*   A USB HID Joystick (Thrustmaster T.16000M).
*   A passive USB-C to USB-A adapter (if using a USB-C board port).
*   MIDI gear connected to the designated UART (e.g., MikroBUS TX).

Building and Running
********************

Build the sample for the FRDM-RW612:

.. code-block:: bash

   west build -p always -b frdm_rw612 samples/midi1_joystick

Sample Output
*************

When the joystick is moved, the console displays the translated values:

.. code-block:: console

   STICK X:7862  | MIDI -> PitchWheel:7862 
   STICK Y:7109  | MIDI -> ModWheel:16 
   STICK X:7935  | MIDI -> PitchWheel:7935 
   STICK Y:7346  | MIDI -> ModWheel:13 
   STICK X:7992  | MIDI -> PitchWheel:7992 
   STICK Y:7595  | MIDI -> ModWheel:9  
   STICK X:8192  | MIDI -> PitchWheel:8192 
   STICK Y:8045  | MIDI -> ModWheel:2  
   STICK Y:8192  | MIDI -> ModWheel:0  
   STICK FADER:237    | MIDI -> VOL:9  
   STICK FADER:203    | MIDI -> VOL:26
