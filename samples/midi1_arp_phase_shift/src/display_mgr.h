/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DISPLAY_MGR_H_
#define DISPLAY_MGR_H_

#include "arp.h"
#include "tempo.h"

/**
 * @file display_mgr.h
 * @brief Modular Display Manager for Arpeggiator Visualization.
 *
 * @author Jan-Willem Smaal <usenet@gispen.org>
 */

/**
 * @brief Initialize the display manager and start the background update thread.
 *
 * @param arp The arpeggiator context to monitor.
 * @param tempo The tempo context to monitor.
 * @return 0 on success, negative errno on failure.
 */
int display_mgr_init(struct arp_ctx *arp, struct tempo_ctx *tempo);

#endif /* DISPLAY_MGR_H_ */
