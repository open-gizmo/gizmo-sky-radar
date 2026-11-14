#include "theme.h"

namespace {
	// Green, like an old radar tube
	constexpr RadarPalette PHOSPHOR = { 0x04170B, 0x000402, 0x0F4A24, 0x1E8443, 0xB4FFD0, 0x52A673, 0x3CFF82, 0xF0FFF5, 0xFFB020 };

	// Orange, like an amber CRT
	constexpr RadarPalette AMBER = { 0x170C02, 0x040100, 0x553108, 0x96590E, 0xFFDDA8, 0xB27C34, 0xFFAE1F, 0xFFF4E0, 0xFF5A3C };

	// Cyan and white on deep blue
	constexpr RadarPalette ARCTIC = { 0x061527, 0x01050C, 0x14395A, 0x2667A0, 0xD4F1FF, 0x6FA5C8, 0x52D9FF, 0xFFFFFF, 0xFFB020 };
}

// The palette of a theme
const RadarPalette &getRadarPalette(RadarTheme theme) {
	switch (theme) {
		case RadarTheme::Amber:
			return AMBER;
		case RadarTheme::Arctic:
			return ARCTIC;
		default:
			return PHOSPHOR;
	}
}

// Color of an aircraft from its altitude
uint32_t getAltitudeColor(bool hasAltitude, bool isOnGround, int32_t altitudeFt) {
	if (isOnGround) {
		return 0x9A9A9A;
	}
	if (!hasAltitude) {
		return 0xDDDDDD;
	}
	if (altitudeFt < 2000) {
		return 0xFF5E4D;
	}
	if (altitudeFt < 10000) {
		return 0xFFA43D;
	}
	if (altitudeFt < 20000) {
		return 0xFFE45A;
	}
	if (altitudeFt < 30000) {
		return 0x86FF5C;
	}
	if (altitudeFt < 40000) {
		return 0x4FD8FF;
	}
	return 0xC58CFF;
}

// Mix two colors
uint32_t mixColors(uint32_t from, uint32_t to, uint8_t amount) {
	const uint32_t inverse = 255 - amount;
	const uint32_t red = (((from >> 16) & 0xFF) * inverse + ((to >> 16) & 0xFF) * amount) / 255;
	const uint32_t green = (((from >> 8) & 0xFF) * inverse + ((to >> 8) & 0xFF) * amount) / 255;
	const uint32_t blue = ((from & 0xFF) * inverse + (to & 0xFF) * amount) / 255;
	return (red << 16) | (green << 8) | blue;
}
