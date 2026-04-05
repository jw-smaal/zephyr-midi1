# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_MIDI1_SERIAL midi1_serial.c)
zephyr_library_link_libraries(midi1_common)
