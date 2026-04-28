# SPDX-License-Identifier: Apache-2.0

if(CONFIG_MIDI1_CLOCK_MEAS_CNTR)
  zephyr_library()
  zephyr_library_sources(midi1_clock_meas_cntr.c midi1_blockavg.c)
  zephyr_library_link_libraries(midi1_common)
endif()
