/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#pragma once

// LEDs definitions for Adafruit BLE friend
#define LEDS_NUMBER    2

#define LED_RGB_RED    38
#define LED_RGB_GREEN  0
#define LED_RGB_BLUE   39

#define LED_RGB_RED_MASK    (1<<LED_RGB_RED)
#define LED_RGB_GREEN_MASK  (1<<LED_RGB_GREEN)
#define LED_RGB_BLUE_MASK   (1<<LED_RGB_BLUE)

#define LEDS_LIST { LED_RGB_RED, LED_RGB_GREEN, LED_RGB_BLUE}
// defining RGB led as 3 single LEDs
#define BSP_LED_0 LED_RGB_RED
#define BSP_LED_1 LED_RGB_GREEN
#define BSP_LED_2 LED_RGB_BLUE

//TODO: UART connection with J-Link
#define RX_PIN_NUMBER  11
#define TX_PIN_NUMBER  9
#define CTS_PIN_NUMBER 10
#define RTS_PIN_NUMBER 8
#define HWFC           true

