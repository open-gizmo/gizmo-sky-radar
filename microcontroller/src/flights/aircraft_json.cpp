#include "aircraft_json.h"
#include <math.h>

namespace {
	// Round a number to some decimals, so the JSON stays short (5 decimals of a degree are about a meter)
	double roundTo(double value, int decimals) {
		const double factor = pow(10, decimals);
		return round(value * factor) / factor;
	}

	// Write a text, or null when it's empty
	void writeText(JsonObject json, const char *key, const char *text) {
		if (text[0] == '\0') {
			json[key] = nullptr;
		} else {
			json[key] = JsonString(text, JsonString::Copied);
		}
	}
}

// Write one aircraft
void writeAircraftToJson(JsonObject json, const AircraftTracker &tracker, const TrackedAircraft &tracked, uint32_t nowMs, uint16_t rangeKm, bool includeTrail) {
	const Aircraft &aircraft = tracked.aircraft;

	// Identity
	json["hex"] = JsonString(aircraft.hex, JsonString::Copied);
	writeText(json, "callsign", aircraft.callsign);
	writeText(json, "registration", aircraft.registration);
	writeText(json, "type", aircraft.typeCode);
	writeText(json, "description", aircraft.description);
	writeText(json, "squawk", aircraft.squawk);
	writeText(json, "category", aircraft.category);

	// Last position, and how old it is now (a client can move it on along the track, like the display)
	json["latitude"] = roundTo(aircraft.position.latitude, 5);
	json["longitude"] = roundTo(aircraft.position.longitude, 5);
	const uint32_t ageMs = nowMs - tracked.positionTimeMs;
	json["positionAgeSeconds"] = ageMs < 0x80000000UL ? roundTo(ageMs / 1000.0, 1) : 0.0;

	// Flight data (null when the aircraft doesn't send it)
	json["onGround"] = aircraft.isOnGround;
	if (aircraft.hasAltitude) {
		json["altitudeFt"] = aircraft.altitudeFt;
	} else {
		json["altitudeFt"] = nullptr;
	}
	if (aircraft.hasGroundSpeed) {
		json["groundSpeedKt"] = roundTo(aircraft.groundSpeedKt, 1);
	} else {
		json["groundSpeedKt"] = nullptr;
	}
	if (aircraft.hasTrack) {
		json["trackDegrees"] = roundTo(aircraft.trackDegrees, 1);
	} else {
		json["trackDegrees"] = nullptr;
	}
	if (aircraft.hasVerticalRate) {
		json["verticalRateFpm"] = aircraft.verticalRateFpm;
	} else {
		json["verticalRateFpm"] = nullptr;
	}
	json["emergency"] = aircraft.isEmergency;

	// Place from the radar, as of the last download
	json["distanceKm"] = roundTo(aircraft.distanceKm, 2);
	json["bearingDegrees"] = roundTo(aircraft.bearingDegrees, 1);
	json["inRange"] = aircraft.distanceKm <= rangeKm;
	json["missing"] = isAircraftMissing(tracker, tracked); // Not in the last download: kept a little while on its last course

	// Earlier positions, oldest first, as [latitude, longitude] pairs
	if (includeTrail) {
		JsonArray trail = json["trail"].to<JsonArray>();
		for (uint8_t index = 0; index < tracked.trailCount; index++) {
			JsonArray point = trail.add<JsonArray>();
			point.add(roundTo(tracked.trail[index].latitude, 5));
			point.add(roundTo(tracked.trail[index].longitude, 5));
		}
	}
}
