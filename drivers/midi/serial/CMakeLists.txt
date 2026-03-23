# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources(midi1_serial.c)
zephyr_library_link_libraries(midi1_common)
