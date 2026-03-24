.. _midi1_parser_tests:

MIDI 1.0 Parser Tests
#####################

:Author: Jan-Willem Smaal <usenet@gispen.org>
:Copyright: (c) Jan-Willem Smaal <usenet@gispen.org>
:License: Apache-2.0

These tests verify the compliance of the MIDI 1.0 serial driver's parser and transmitter
against the official MIDI 1.0 specification.

How to Run Tests
****************

On macOS (using QEMU)
=====================

Since ``native_sim`` is Linux-only, use ``qemu_cortex_m3`` to run the tests on macOS.
You must explicitly point to the module path so Zephyr can find the Kconfig definitions.

Using Twister:

.. code-block:: bash

   west twister -p qemu_cortex_m3 -T tests/lib/midi1_parser -v --west-flash="-DZEPHYR_MODULES=$(pwd)"

Manual Build & Run:

.. code-block:: bash

   west build -b qemu_cortex_m3 tests/lib/midi1_parser -p always -DZEPHYR_MODULES=$(pwd)
   west build -t run

On Linux (using native_sim)
===========================

The tests run significantly faster using the POSIX architecture on Linux:

.. code-block:: bash

   west twister -p native_sim -T tests/lib/midi1_parser -v --west-flash="-DZEPHYR_MODULES=$(pwd)"

What is Verified
****************

The test suite (``src/main.c``) covers the following MIDI 1.0 specification rules:

1. **Basic Note On/Off**: Standard 3-byte message parsing.
2. **Running Status**: Correct parsing when status bytes are omitted between subsequent messages of the same type.
3. **Interleaved Real-time**: Ensuring that Real-time messages (Clock, Start, Stop) can occur anywhere in the data stream without breaking the parsing of Channel Voice messages.
4. **Running Status Reset**: Verifying that System Common messages (e.g., Song Select) correctly reset the parser's running status state.
5. **SysEx Integrity**: Proper handling of System Exclusive start, data, and stop bytes, including interleaved Real-time messages.
6. **TX Status Re-insertion**: Verifying that the transmitter automatically re-inserts a status byte after 15 messages (as configured by ``CONFIG_MIDI1_SERIAL_RUNNING_STATUS_REPEAT``) to ensure synchronization in case of cable disconnects.
