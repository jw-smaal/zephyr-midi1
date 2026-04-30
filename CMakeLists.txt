# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

if(ZEPHYR_BASE)
  zephyr_include_directories(include)

  add_subdirectory(drivers/midi/common)
  add_subdirectory(drivers/midi/serial)
  add_subdirectory(drivers/midi/clock)
  add_subdirectory(drivers/midi/measurement)
  
  if(CONFIG_MIDI1_HARMONIC)
    add_subdirectory(lib/harmonic)
  endif()
endif()
