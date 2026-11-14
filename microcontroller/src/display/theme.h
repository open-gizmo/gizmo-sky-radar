#ifndef THEME_H
#define THEME_H

#include <stdint.h>
#include "../config/radar_settings.h"

// Colors of the radar screen, as RGB888 (the display converts them to its RGB565)

// Colors of one theme
struct RadarPalette {
	uint32_t background; // Center of the screen
	uint32_t backgroundEdge; // Edge of the screen (the background darkens toward it)
	uint32_t grid; // Range rings and crosshair
	uint32_t gridBright; // Outer ring and the ticks of the bezel
	uint32_t text; // Main texts
	uint32_t textDim; // Secondary texts
	uint32_t sweep; // Beam of the radar
	uint32_t accent; // The home marker and the spotlight on an aircraft
	uint32_t warning; // Old data, errors
};

// The palette of a theme
const RadarPalette &getRadarPalette(RadarTheme theme);

// Color of an aircraft from its altitude (the same in every theme, and in the web app): grey on the ground, red low, through orange, yellow, green and cyan, to violet high up
uint32_t getAltitudeColor(bool hasAltitude, bool isOnGround, int32_t altitudeFt);

// Color of an emergency
constexpr uint32_t EMERGENCY_COLOR = 0xFF2D2D;

// Mix two colors: amount 0 is the first, 255 the second
uint32_t mixColors(uint32_t from, uint32_t to, uint8_t amount);

#endif
