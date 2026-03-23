# Zephyr MIDI1 Module

A standalone Zephyr RTOS module providing MIDI 1.0 drivers and protocol helpers.

**Author:** Jan-Willem Smaal <usenet@gispen.org>  
**Copyright:** (c) Jan-Willem Smaal <usenet@gispen.org>  
**License:** Apache-2.0

## Features

- **MIDI Serial UART Driver**: Supports MIDI 1.0 over UART with "Running Status" support (transmit and receive) and thread-safe mutex protection for multi-threaded applications.
- **MIDI Clock Generator**: Hardware-based MIDI clock (24 PPQN) generation.
- **MIDI Clock Measurement**: Measures BPM of incoming clock pulses with block averaging.
- **Universal MIDI Packet (UMP)**: Helpers for generating MIDI 1.0 UMP packets.

## Integration via West

To use this module in your Zephyr project, add it to your `west.yml` manifest:

```yaml
manifest:
  projects:
    - name: zephyr
      remote: zephyrproject-rtos
      revision: main
      import: true

    - name: zephyr-midi1
      url: https://github.com/jw-smaal/zephyr-midi1
      revision: main
      path: modules/lib/midi1
```

Then run:
```bash
west update
```

## Configuration

Enable the desired drivers in your project's `prj.conf`:

```kconfig
# Enable MIDI Serial UART Driver
CONFIG_MIDI1_SERIAL=y

# Enable MIDI Clock Generator
CONFIG_MIDI1_CLOCK_CNTR=y

# Enable MIDI Clock Measurement
CONFIG_MIDI1_CLOCK_MEAS_CNTR=y
```

## Devicetree Usage

Example for defining a MIDI serial instance in your Devicetree overlay:

```dts
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
 * Because on the FRDM_K64F each channel has it's own IRQ we need to be 
 * specific here can't just enable pit0 and hope for the best. 
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
```

## License

Distributed under the Apache License, Version 2.0. See the source files for details.
