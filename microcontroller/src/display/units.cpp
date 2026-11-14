#include "units.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

namespace {
	constexpr double KM_PER_NAUTICAL_MILE = 1.852;
	constexpr double KM_PER_MILE = 1.609344;
	constexpr double METERS_PER_FOOT = 0.3048;

	// Altitude from which aviation units give a flight level (the transition altitude of the United States, the highest common one)
	constexpr int32_t FLIGHT_LEVEL_FROM_FT = 18000;

	// Write a whole number with thousands separators ("12,500")
	void formatThousands(char *text, size_t size, long value) {
		const long absoluteValue = labs(value);
		if (absoluteValue >= 1000) {
			snprintf(text, size, "%s%ld,%03ld", value < 0 ? "-" : "", absoluteValue / 1000, absoluteValue % 1000);
		} else {
			snprintf(text, size, "%ld", value);
		}
	}

	// A distance in kilometers, in the unit of the system
	double convertDistance(double distanceKm, UnitSystem units) {
		switch (units) {
			case UnitSystem::Metric:
				return distanceKm;
			case UnitSystem::Imperial:
				return distanceKm / KM_PER_MILE;
			default:
				return distanceKm / KM_PER_NAUTICAL_MILE;
		}
	}

	// Name of the distance unit of the system
	const char *getDistanceUnit(UnitSystem units) {
		switch (units) {
			case UnitSystem::Metric:
				return "km";
			case UnitSystem::Imperial:
				return "mi";
			default:
				return "nm";
		}
	}
}

// Altitude, for the details of an aircraft
void formatAltitude(char *text, size_t size, bool hasAltitude, bool isOnGround, int32_t altitudeFt, UnitSystem units) {
	// On the ground, or no altitude at all
	if (isOnGround) {
		snprintf(text, size, "GND");
		return;
	}
	if (!hasAltitude) {
		snprintf(text, size, "---");
		return;
	}

	// Meters
	char number[16];
	if (units == UnitSystem::Metric) {
		formatThousands(number, sizeof(number), lround(altitudeFt * METERS_PER_FOOT));
		snprintf(text, size, "%s m", number);
		return;
	}

	// Flight levels high up, for pilots
	if (units == UnitSystem::Aviation && altitudeFt >= FLIGHT_LEVEL_FROM_FT) {
		snprintf(text, size, "FL%03ld", lround(altitudeFt / 100.0));
		return;
	}

	// Feet
	formatThousands(number, sizeof(number), altitudeFt);
	snprintf(text, size, "%s ft", number);
}

// Short altitude for the label next to an aircraft
void formatShortAltitude(char *text, size_t size, bool hasAltitude, bool isOnGround, int32_t altitudeFt, UnitSystem units) {
	if (isOnGround) {
		snprintf(text, size, "GND");
		return;
	}
	if (!hasAltitude) {
		snprintf(text, size, "%s", "");
		return;
	}

	// Hundreds of meters, or hundreds of feet (the flight level, like a controller's screen)
	const double hundreds = units == UnitSystem::Metric ? altitudeFt * METERS_PER_FOOT / 100.0 : altitudeFt / 100.0;
	snprintf(text, size, "%03ld", lround(hundreds < 0 ? 0 : hundreds));
}

// Distance, one decimal under 10
void formatDistance(char *text, size_t size, double distanceKm, UnitSystem units) {
	const double distance = convertDistance(distanceKm, units);
	if (distance < 10) {
		snprintf(text, size, "%.1f %s", distance, getDistanceUnit(units));
	} else {
		snprintf(text, size, "%ld %s", lround(distance), getDistanceUnit(units));
	}
}

// Speed
void formatSpeed(char *text, size_t size, float speedKt, UnitSystem units) {
	switch (units) {
		case UnitSystem::Metric:
			snprintf(text, size, "%ld km/h", lround(speedKt * KM_PER_NAUTICAL_MILE));
			break;
		case UnitSystem::Imperial:
			snprintf(text, size, "%ld mph", lround(speedKt * KM_PER_NAUTICAL_MILE / KM_PER_MILE));
			break;
		default:
			snprintf(text, size, "%ld kt", lround(speedKt));
			break;
	}
}

// Range of the outer ring
void formatRange(char *text, size_t size, uint16_t rangeKm, UnitSystem units) {
	const double range = convertDistance(rangeKm, units);
	if (range < 10 && fabs(range - lround(range)) >= 0.05) {
		snprintf(text, size, "%.1f %s", range, getDistanceUnit(units));
	} else {
		snprintf(text, size, "%ld %s", lround(range), getDistanceUnit(units));
	}
}

// Compass direction of a bearing
const char *getCompassPoint(double bearingDegrees) {
	constexpr const char *POINTS[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
	double bearing = fmod(bearingDegrees, 360.0);
	if (bearing < 0) {
		bearing += 360.0;
	}
	return POINTS[static_cast<int>((bearing + 22.5) / 45.0) % 8];
}
