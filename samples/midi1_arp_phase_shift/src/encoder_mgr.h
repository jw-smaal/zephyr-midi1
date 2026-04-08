/*
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file encoder_mgr.h
 * @brief Rotary Encoder Manager for Arpeggiator Parameter Control.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

#ifndef ENCODER_MGR_H_
#define ENCODER_MGR_H_

#include "arp.h"
#include "tempo.h"

/**
 * @brief Parameters that can be controlled by the rotary encoder.
 */
enum encoder_param {
	ENCODER_PARAM_DRIFT = 0, /* Phase Drift Cycle */
	ENCODER_PARAM_MODE = 1,  /* Arpeggiator Mode */
	ENCODER_PARAM_BPM = 2,   /* BPM Adjustment */
	ENCODER_PARAM_POLY_OFFSET = 3, /* Polymetric Length Offset */
	ENCODER_PARAM_MAX = 4
};

/**
 * @brief Initialize the rotary encoder manager.
 *
 * @param arp Arpeggiator context to control.
 * @param tempo Tempo context to control.
 * @return 0 on success, negative error code on failure.
 */
int encoder_mgr_init(struct arp_ctx *arp, struct tempo_ctx *tempo);

/**
 * @brief Get the currently selected parameter for the encoder.
 */
enum encoder_param encoder_mgr_get_current_param(void);

/**
 * @brief Get the 'pending' mode that is selected but not yet applied.
 */
enum arp_mode encoder_mgr_get_pending_mode(void);

/**
 * @brief Check if the 'Shift' button (SW2) is currently held down.
 */
bool encoder_mgr_is_shift_active(void);

#endif /* ENCODER_MGR_H_ */
