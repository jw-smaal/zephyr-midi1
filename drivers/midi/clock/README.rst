MIDI Clock Generator Driver
##########################

This driver generates a stable MIDI 1.0 timing clock (24 PPQN) at a user-defined tempo. It leverages the Zephyr **Counter API** to ensure precise timing across different hardware platforms.

Overview
********

The ``midi1_clock_cntr`` driver provides a high-level interface for generating MIDI clock pulses. It calculates the necessary hardware timer intervals based on a **Scaled BPM (SBPM)** value, where 120.00 BPM is represented as ``12000``.

Features
********

- **24 PPQN Resolution:** Automatically handles the pulses-per-quarter-note math required by the MIDI 1.0 specification.
- **Scaled BPM (SBPM):** Supports fractional tempo (e.g., 120.05 BPM) without floating-point math.
- **Hardware Agnostic:** Works with any timer supported by the Zephyr Counter API (e.g., NXP PIT, LPTIMER, etc.).
- **Jitter-Free Sync:** Uses periodic counter interrupts to maintain timing consistency.

Devicetree Configuration
************************

To use this driver, you must define a ``midi1_clock_cntr`` node in your Devicetree and link it to a hardware counter and a MIDI serial device.

.. code-block:: dts

   / {
       midi_clock: midi1-clock-cntr {
           compatible = "midi1_clock_cntr";
           counter = <&pit0_channel0>;
           midi1_serial = <&midi_serial_0>;
           status = "okay";
       };
   };

   &pit0_channel0 {
       status = "okay";
   };

API Usage
*********

Generating a Clock
==================

.. code-block:: c

   const struct device *clk_dev = DEVICE_DT_GET_ANY(midi1_clock_cntr);
   
   /* Generate a 120.00 BPM clock */
   uint16_t target_sbpm = 12000; 
   midi1_clock_cntr_gen(clk_dev, target_sbpm);

Stopping the Clock
==================

.. code-block:: c

   midi1_clock_cntr_stop(clk_dev);

Configuration
*************

The driver can be configured via Kconfig:

- ``CONFIG_MIDI1_CLOCK_CNTR``: Enables the driver.
- ``CONFIG_MIDI1_CLOCK_CNTR_INIT_PRIORITY``: Set the initialization priority (defaults to ``POST_KERNEL``).
