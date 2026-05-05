/*
 * SPDX-License-Identifier: Apache-2.0
 */
 
/*
 * @author Jan-Willem Smaal <usenet@gispen.org>
 * @file display.c
 * @date 20260504
 */ 
#ifndef DISPLAY_H
#define DISPLAY_H

#include <zephyr/logging/log.h>
#include <string.h>
#include <lvgl.h>



void update_chord_display(char * buf, uint8_t len);

#endif 
/* EOF */