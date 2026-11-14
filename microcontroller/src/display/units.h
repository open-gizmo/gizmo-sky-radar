#ifndef UNITS_H
#define UNITS_H

#include <stddef.h>
#include <stdint.h>
#include "../config/radar_settings.h"

// The numbers of the radar in the units the user picked, as short texts for the small round screen, kept free of hardware headers so the native tests can use them

// Altitude: flight levels above 18,000 ft in aviation units ("FL350"), "12,500 ft" or "3,810 m" otherwise, "GND" on the ground
void formatAltitude(char *text, size_t size, bool hasAltitude, bool isOnGround, int32_t altitudeFt, UnitSystem units);

// Short altitude for the label next to an aircraft: hundreds of feet ("350", "45") or of meters ("107"), "GND" on the ground, "" without one
void formatShortAltitude(char *text, size_t size, bool hasAltitude, bool isOnGround, int32_t altitudeFt, UnitSystem units);

// Distance: "12.4 nm", "23 km" (one decimal under 10)
void formatDistance(char *text, size_t size, double distanceKm, UnitSystem units);

// Speed: "450 kt", "833 km/h", "518 mph"
void formatSpeed(char *text, size_t size, float speedKt, UnitSystem units);

// Range of the outer ring, without decimals when it's a whole number: "40 km", "22 nm", "25 mi"
void formatRange(char *text, size_t size, uint16_t rangeKm, UnitSystem units);

// Compass direction of a bearing: N, NE, E, SE, S, SW, W or NW
const char *getCompassPoint(double bearingDegrees);

#endif
