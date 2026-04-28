# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MIDI1_SERIAL)
  zephyr_library()
  zephyr_library_sources(midi1_serial.c)
  zephyr_library_link_libraries(midi1_common)
endif()
