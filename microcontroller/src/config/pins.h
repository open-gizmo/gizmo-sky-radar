#ifndef PINS_H
#define PINS_H

#include <stdint.h>

// Pins of the GC9A01 round display on the Seeed Studio XIAO ESP32-S3 (the pad names are printed on the board)
#if defined(ARDUINO_XIAO_ESP32S3)
constexpr int8_t DISPLAY_MOSI_PIN = 9; // DIN, pad D10 (MOSI)
constexpr int8_t DISPLAY_SCLK_PIN = 7; // CLK, pad D8 (SCK)
constexpr int8_t DISPLAY_CS_PIN = 2; // CS, pad D1
constexpr int8_t DISPLAY_DC_PIN = 3; // DC, pad D2
constexpr int8_t DISPLAY_RST_PIN = 4; // RST, pad D3
constexpr int8_t DISPLAY_BL_PIN = 1; // BL, pad D0 (the backlight, dimmed with PWM)
#else
#error "The sky radar runs on the Seeed Studio XIAO ESP32-S3 (pio run -e release)"
#endif

#endif
