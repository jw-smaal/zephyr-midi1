/*
 * SPDX-License-Identifier: Apache-2.0
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @file chord_detector.h
 * @date 20260504
 */

#ifndef CHORD_DETECTOR_H_
#define CHORD_DETECTOR_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/midi/midi1_serial.h>

/**
 * @brief Initialize the chord detector.
 * 
 * @param dev The MIDI device to listen on.
 * @return 0 on success, negative error code otherwise.
 */
int chord_detector_init(const struct device *dev);

/**
 * @brief Get the registered MIDI callbacks for the chord detector.
 * 
 * @return Pointer to the callbacks structure.
 */
const struct midi1_serial_callbacks *chord_detector_get_callbacks(void);

/**
 * @brief Get the last recognized chord name.
 * 
 * @return String containing the chord name.
 */
const char *chord_detector_get_last_name(void);

#endif /* CHORD_DETECTOR_H_ */
