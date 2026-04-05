# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_MIDI1_CLOCK_CNTR midi1_clock_cntr.c)
zephyr_library_link_libraries(midi1_common)
