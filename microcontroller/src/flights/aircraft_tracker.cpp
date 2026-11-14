#include "aircraft_tracker.h"
#include <string.h>

namespace {
	// Find an aircraft of the tracker by its ICAO address (-1 when it isn't followed)
	int findTracked(const TrackedAircraft *items, size_t count, const char *hex) {
		for (size_t index = 0; index < count; index++) {
			if (strcmp(items[index].aircraft.hex, hex) == 0) {
				return static_cast<int>(index);
			}
		}
		return -1;
	}

	// Add a position to the end of a trail, dropping the oldest when it is full (not when it barely moved)
	void appendTrailPoint(TrackedAircraft &tracked, const GeoPoint &point) {
		if (tracked.trailCount > 0 && getDistanceKm(tracked.trail[tracked.trailCount - 1], point) < TRAIL_MIN_SPACING_KM) {
			return;
		}
		if (tracked.trailCount == TRAIL_LENGTH) {
			memmove(&tracked.trail[0], &tracked.trail[1], sizeof(GeoPoint) * (TRAIL_LENGTH - 1));
			tracked.trailCount--;
		}
		tracked.trail[tracked.trailCount++] = point;
	}

	// The merged list, built away from the tracker (static: too big for the stack of a task)
	TrackedAircraft mergedItems[MAX_AIRCRAFT];
}

// Forget every aircraft
void resetTracker(AircraftTracker &tracker) {
	tracker.count = 0;
	tracker.lastUpdateMs = 0;
}

// Take in a new download
void updateTracker(AircraftTracker &tracker, const Aircraft *aircraft, size_t count, uint32_t downloadTimeMs) {
	size_t mergedCount = 0;

	// The aircraft of the download, nearest first: the known ones keep their trail
	for (size_t index = 0; index < count && mergedCount < MAX_AIRCRAFT; index++) {
		TrackedAircraft &merged = mergedItems[mergedCount++];
		const int previousIndex = findTracked(tracker.items, tracker.count, aircraft[index].hex);
		if (previousIndex >= 0) {
			merged = tracker.items[previousIndex];
		} else {
			merged.trailCount = 0;
		}
		merged.aircraft = aircraft[index];
		merged.lastSeenMs = downloadTimeMs;
		merged.positionTimeMs = downloadTimeMs - static_cast<uint32_t>(aircraft[index].positionAgeSeconds * 1000.0f);
		appendTrailPoint(merged, aircraft[index].position);
	}

	// The ones missing from the download stay a while, while there is room (a missed download, or a gap in the coverage)
	for (size_t index = 0; index < tracker.count && mergedCount < MAX_AIRCRAFT; index++) {
		const TrackedAircraft &previous = tracker.items[index];
		if (findTracked(mergedItems, mergedCount, previous.aircraft.hex) >= 0 || downloadTimeMs - previous.lastSeenMs >= AIRCRAFT_MISSING_TIMEOUT_MS) {
			continue;
		}

		// Insert it by its last distance, so the list stays nearest first
		size_t position = mergedCount;
		while (position > 0 && mergedItems[position - 1].aircraft.distanceKm > previous.aircraft.distanceKm) {
			mergedItems[position] = mergedItems[position - 1];
			position--;
		}
		mergedItems[position] = previous;
		mergedCount++;
	}

	// Keep the merged list
	memcpy(tracker.items, mergedItems, sizeof(TrackedAircraft) * mergedCount);
	tracker.count = mergedCount;
	tracker.lastUpdateMs = downloadTimeMs;
}

// Where an aircraft is estimated to be now
GeoPoint getEstimatedPosition(const TrackedAircraft &tracked, uint32_t nowMs) {
	const Aircraft &aircraft = tracked.aircraft;

	// Without a course (or on the ground, where it turns at every taxiway), its last position
	if (aircraft.isOnGround || !aircraft.hasGroundSpeed || !aircraft.hasTrack || aircraft.groundSpeedKt <= 0) {
		return aircraft.position;
	}

	// Move it along its track for the time since its position (capped, the guess drifts as time goes)
	uint32_t elapsedMs = nowMs - tracked.positionTimeMs;
	if (elapsedMs > 0x80000000UL) {
		elapsedMs = 0; // A position from the future (clock rounding): don't move it back
	}
	if (elapsedMs > MAX_DEAD_RECKONING_MS) {
		elapsedMs = MAX_DEAD_RECKONING_MS;
	}
	const double distanceKm = aircraft.groundSpeedKt * NAUTICAL_MILE_KM * (elapsedMs / 3600000.0);
	return movePoint(aircraft.position, aircraft.trackDegrees, distanceKm);
}

// Whether an aircraft is no longer in the downloads
bool isAircraftMissing(const AircraftTracker &tracker, const TrackedAircraft &tracked) {
	return tracked.lastSeenMs != tracker.lastUpdateMs;
}
