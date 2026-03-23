# SPDX-License-Identifier: Apache-2.0

# MIDI 1.0 Clock Measurement Sample
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(midi1_clock_measurement)

target_sources(app PRIVATE src/main.c)
