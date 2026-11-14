#ifndef AIRCRAFT_TRACKER_H
#define AIRCRAFT_TRACKER_H

#include <stddef.h>
#include <stdint.h>
#include "aircraft.h"

// Following the aircraft from one download to the next: their trails, their estimated position between downloads, and the ones that briefly drop out of the data
// Kept free of hardware headers so the native tests can use it (times are millis(), 32-bit like on the ESP32)

// Points of the trail of an aircraft (about a minute of flight at one download every 5 seconds)
constexpr size_t TRAIL_LENGTH = 12;

// Shortest move between two points of a trail, in kilometers (a parked or hovering aircraft keeps a short trail)
constexpr double TRAIL_MIN_SPACING_KM = 0.08;

// How long an aircraft missing from the data stays on the radar, moving on its last course (a missed download, or a gap in the coverage)
constexpr uint32_t AIRCRAFT_MISSING_TIMEOUT_MS = 20000;

// The longest an aircraft is moved forward from its last position, in milliseconds (past that, the guess would drift too far)
constexpr uint32_t MAX_DEAD_RECKONING_MS = 45000;

// An aircraft followed across downloads
struct TrackedAircraft {
	Aircraft aircraft; // As in the last download it was in
	uint32_t positionTimeMs; // When its position was true (the download time, minus the age of the position)
	uint32_t lastSeenMs; // The last download it was in
	GeoPoint trail[TRAIL_LENGTH]; // Its earlier positions, oldest first
	uint8_t trailCount;
};

// All the aircraft followed, nearest first as of the last download
struct AircraftTracker {
	TrackedAircraft items[MAX_AIRCRAFT];
	size_t count;
	uint32_t lastUpdateMs; // When the last download was taken in
};

// Forget every aircraft (after a change of location, for example)
void resetTracker(AircraftTracker &tracker);

// Take in a new download: known aircraft keep their trail, new ones start one, missing ones stay a while; the list is sorted nearest first again
void updateTracker(AircraftTracker &tracker, const Aircraft *aircraft, size_t count, uint32_t downloadTimeMs);

// Where an aircraft is estimated to be now, from its last position, speed and track (dead reckoning; on the ground or without a course, its last position)
GeoPoint getEstimatedPosition(const TrackedAircraft &tracked, uint32_t nowMs);

// Whether an aircraft is no longer in the downloads (it is kept a little while, moving on its last course)
bool isAircraftMissing(const AircraftTracker &tracker, const TrackedAircraft &tracked);

#endif
