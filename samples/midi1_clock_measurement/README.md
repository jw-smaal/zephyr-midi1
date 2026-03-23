# MIDI 1.0 Clock Measurement Sample
**License:** Apache-2.0  
**Author:** Jan-Willem Smaal <usenet@gispen.org>  
**Copyright:** (c) Jan-Willem Smaal <usenet@gispen.org>

A hardware-based MIDI 1.0 timing clock measurement implementation for Zephyr RTOS.

## Overview
This sample shows how to use the `midi1_clock_meas_cntr` driver to measure the BPM of an incoming MIDI timing clock signal. It uses block averaging to provide a stable BPM reading.

## Features
- Accurately measures incoming 24 PPQN signals.
- Block averaging to reduce measurement jitter.
- Uses `sbpm_to_str()` for human-readable tempo display.

## Hardware Requirements
Requires a board with a counter device (timer) supporting external input or capture.
Tested on:
- **NXP FRDM-K64F** (PIT timer)
- **NXP FRDM-MCXC242** (LPTIMER/PIT)

## Building
From your Zephyr workspace:

```bash
west build -b <your_board> modules/lib/midi1/samples/midi1_clock_measurement
```

## Running
Once flashed, the sample will monitor the incoming timing clock and report the measured BPM every 2 seconds via the console.

## License
Distributed under the Apache License, Version 2.0.
