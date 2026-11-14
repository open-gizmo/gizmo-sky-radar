#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include <stddef.h>
#include <stdint.h>
#include "geo.h"

// An aircraft as the radar knows it, kept free of hardware headers so the native tests can use it

// Sizes of the texts, each with its terminator
constexpr size_t AIRCRAFT_HEX_SIZE = 7; // ICAO 24-bit address, for example "4b1814"
constexpr size_t AIRCRAFT_CALLSIGN_SIZE = 9; // For example "SWR123A"
constexpr size_t AIRCRAFT_REGISTRATION_SIZE = 13; // For example "HB-JCA"
constexpr size_t AIRCRAFT_TYPE_SIZE = 5; // ICAO type designator, for example "A320"
constexpr size_t AIRCRAFT_DESCRIPTION_SIZE = 33; // For example "AIRBUS A-320"
constexpr size_t AIRCRAFT_SQUAWK_SIZE = 5; // For example "7000"
constexpr size_t AIRCRAFT_CATEGORY_SIZE = 3; // Emitter category, for example "A3"

// Most aircraft the radar keeps, the nearest ones (enough for a busy sky, small enough for the screen)
constexpr size_t MAX_AIRCRAFT = 64;

// One aircraft from the flight data
struct Aircraft {
	char hex[AIRCRAFT_HEX_SIZE];
	char callsign[AIRCRAFT_CALLSIGN_SIZE]; // Empty when the aircraft doesn't send one
	char registration[AIRCRAFT_REGISTRATION_SIZE];
	char typeCode[AIRCRAFT_TYPE_SIZE];
	char description[AIRCRAFT_DESCRIPTION_SIZE];
	char squawk[AIRCRAFT_SQUAWK_SIZE];
	char category[AIRCRAFT_CATEGORY_SIZE];
	GeoPoint position; // The last position the aircraft sent
	float positionAgeSeconds; // How old the position was when the data was fetched
	bool isOnGround;
	bool hasAltitude;
	int32_t altitudeFt; // Barometric altitude (the GPS one when the barometric one is missing)
	bool hasGroundSpeed;
	float groundSpeedKt;
	bool hasTrack;
	float trackDegrees; // Direction of travel over the ground, clockwise from true north
	bool hasVerticalRate;
	int16_t verticalRateFpm; // Feet per minute, positive when climbing
	bool isEmergency; // An emergency, or the squawk of a hijack, radio failure or emergency (7500, 7600, 7700)
	double distanceKm; // From the center of the radar
	double bearingDegrees; // From the center of the radar, clockwise from true north
};

// Name to show for an aircraft: its callsign, else its registration, else its ICAO address
const char *getAircraftDisplayName(const Aircraft &aircraft);

#endif
