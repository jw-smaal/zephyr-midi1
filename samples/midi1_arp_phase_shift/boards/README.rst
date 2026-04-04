Hardware Support
################

This directory contains Devicetree overlays for the various boards supported by the Phasing Arpeggiator. Each overlay configures the necessary MIDI UART, Hardware Counters, and ADC inputs for BPM control.

ADC Potentiometer Connections
*****************************

To control the BPM dynamically, connect a 10k Ohm potentiometer to the following pins (mapped to the ``zephyr,user`` node):

+-----------------------+--------------+---------+-----------------------------+
| Board                 | ADC Instance | Channel | Pin (Header)                |
+=======================+==============+=========+=============================+
| **FRDM-MCXC242**      | ``adc0``     | 1       | **A1** (PTE16)              |
+-----------------------+--------------+---------+-----------------------------+
| **FRDM-K64F**         | ``adc0``     | 14      | **A1** (PTC0)               |
+-----------------------+--------------+---------+-----------------------------+
| **FRDM-MCXA156**      | ``lpadc0``   | 0       | **A0** (P2_0)               |
+-----------------------+--------------+---------+-----------------------------+
| **FRDM-MCXC444**      | ``adc0``     | 0       | **A0** (PTE20)              |
+-----------------------+--------------+---------+-----------------------------+
| **FRDM-MCXW71**       | ``adc0``     | 6       | **A0** (PTD2)               |
+-----------------------+--------------+---------+-----------------------------+
| **FRDM-RW612**        | ``adc0``     | 0       | **A0** (GAU_CH0)            |
+-----------------------+--------------+---------+-----------------------------+
| **nRF5340 DK**        | ``adc``      | 0       | **A0** (AIN1)               |
+-----------------------+--------------+---------+-----------------------------+
| **Nucleo F030R8**     | ``adc1``     | 0       | **A0** (PA0)                |
+-----------------------+--------------+---------+-----------------------------+

User Interface Controls
***********************

+-----------------+-----------------------------------------------------------+
| Control         | Function                                                  |
+=================+===========================================================+
| **SW2 Button**  | **Toggle Latch:** Keeps the arpeggio playing after keys   |
|                 | are released. Clears chord on new Note On.                |
+-----------------+-----------------------------------------------------------+
| **SW3 Button**  | **Mode Selector:** Cycles through SYNC, PHASE, PROCESS,   |
|                 | and PHASE+PROCESS modes.                                  |
+-----------------+-----------------------------------------------------------+
| **Red LED**     | **Latch Indicator:** Illuminates when Latch is active.    |
+-----------------+-----------------------------------------------------------+
| **Green LED**   | **Mode Indicator:** Illuminates when in non-SYNC modes.   |
+-----------------+-----------------------------------------------------------+
| **Blue LED**    | **Tempo Heartbeat:** Flashes on every quarter note.       |
+-----------------+-----------------------------------------------------------+

.. note::
   On boards with limited LEDs or Buttons (e.g., FRDM-MCXW71), multiple 
   aliases may map to the same physical hardware component. Check the 
   specific overlay for detailed mapping.
