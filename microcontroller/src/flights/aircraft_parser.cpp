#include "aircraft_parser.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
	// Largest radius adsb.fi answers, in nautical miles
	constexpr uint16_t MAX_QUERY_RADIUS_NM = 250;

	// How much wider than the range the query is, so the aircraft just outside show on the rim
	constexpr double QUERY_RADIUS_FACTOR = 1.3;

	// Copy a text into a fixed field, without its trailing spaces (callsigns come padded to 8 characters), cut to fit
	void copyTrimmed(char *destination, size_t size, const char *text) {
		destination[0] = '\0';
		if (text == nullptr || size == 0) {
			return;
		}

		// Skip the leading spaces, then copy what fits
		while (*text == ' ') {
			text++;
		}
		snprintf(destination, size, "%s", text);

		// Drop the trailing spaces
		size_t length = strlen(destination);
		while (length > 0 && destination[length - 1] == ' ') {
			destination[--length] = '\0';
		}
	}

	// Whether a squawk code means trouble: hijack, radio failure or emergency
	bool isEmergencySquawk(const char *squawk) {
		return strcmp(squawk, "7500") == 0 || strcmp(squawk, "7600") == 0 || strcmp(squawk, "7700") == 0;
	}

	// Read a number field into a float (false when missing or not a number)
	bool readNumber(JsonVariantConst value, float &number) {
		if (!value.is<float>()) {
			return false;
		}
		number = value.as<float>();
		return isfinite(number);
	}
}

// Name to show for an aircraft: its callsign, else its registration, else its ICAO address
const char *getAircraftDisplayName(const Aircraft &aircraft) {
	if (aircraft.callsign[0] != '\0') {
		return aircraft.callsign;
	}
	if (aircraft.registration[0] != '\0') {
		return aircraft.registration;
	}
	return aircraft.hex;
}

// The URL that asks adsb.fi for the aircraft around a point
bool buildFlightDataUrl(char *url, size_t urlSize, const GeoPoint &center, uint16_t radiusNm) {
	const int length = snprintf(url, urlSize, "https://opendata.adsb.fi/api/v3/lat/%.5f/lon/%.5f/dist/%u", center.latitude, center.longitude, static_cast<unsigned>(radiusNm));
	return length > 0 && static_cast<size_t>(length) < urlSize;
}

// The radius to ask for, in nautical miles, for a radar range
uint16_t getQueryRadiusNm(uint16_t rangeKm) {
	const double radiusNm = ceil(rangeKm * QUERY_RADIUS_FACTOR / NAUTICAL_MILE_KM);
	if (radiusNm < 1) {
		return 1;
	}
	return radiusNm > MAX_QUERY_RADIUS_NM ? MAX_QUERY_RADIUS_NM : static_cast<uint16_t>(radiusNm);
}

// Keep only the fields the radar reads
void buildFlightDataFilter(JsonDocument &filter) {
	filter.clear();
	constexpr const char *FIELDS[] = { "hex", "flight", "r", "t", "desc", "squawk", "category", "emergency", "lat", "lon", "seen_pos", "alt_baro", "alt_geom", "gs", "track", "true_heading", "baro_rate", "geom_rate" };

	// The same fields in both lists: "ac" in the v3 API, "aircraft" in the v2 one
	for (const char *list : { "ac", "aircraft" }) {
		JsonObject item = filter[list][0].to<JsonObject>();
		for (const char *field : FIELDS) {
			item[field] = true;
		}
	}
}

// Read one aircraft object
bool parseAircraft(JsonObjectConst object, const GeoPoint &center, Aircraft &aircraft) {
	aircraft = {};

	// Position: without one, the aircraft can't be placed on the radar
	JsonVariantConst latitude = object["lat"];
	JsonVariantConst longitude = object["lon"];
	if (!latitude.is<double>() || !longitude.is<double>()) {
		return false;
	}
	aircraft.position = { latitude.as<double>(), longitude.as<double>() };
	if (!isfinite(aircraft.position.latitude) || !isfinite(aircraft.position.longitude) || fabs(aircraft.position.latitude) > 90 || fabs(aircraft.position.longitude) > 180) {
		return false;
	}
	float positionAge = 0;
	aircraft.positionAgeSeconds = readNumber(object["seen_pos"], positionAge) && positionAge > 0 ? positionAge : 0;
	aircraft.distanceKm = getDistanceKm(center, aircraft.position);
	aircraft.bearingDegrees = getBearingDegrees(center, aircraft.position);

	// Identity
	copyTrimmed(aircraft.hex, sizeof(aircraft.hex), object["hex"] | "");
	copyTrimmed(aircraft.callsign, sizeof(aircraft.callsign), object["flight"] | "");
	copyTrimmed(aircraft.registration, sizeof(aircraft.registration), object["r"] | "");
	copyTrimmed(aircraft.typeCode, sizeof(aircraft.typeCode), object["t"] | "");
	copyTrimmed(aircraft.description, sizeof(aircraft.description), object["desc"] | "");
	copyTrimmed(aircraft.squawk, sizeof(aircraft.squawk), object["squawk"] | "");
	copyTrimmed(aircraft.category, sizeof(aircraft.category), object["category"] | "");

	// Altitude: a number in feet, or "ground" for an aircraft on the ground (the GPS altitude when there is no barometric one)
	JsonVariantConst barometricAltitude = object["alt_baro"];
	if (barometricAltitude.is<const char *>()) {
		aircraft.isOnGround = strcmp(barometricAltitude.as<const char *>(), "ground") == 0;
	} else if (barometricAltitude.is<long>() || barometricAltitude.is<double>()) {
		aircraft.hasAltitude = true;
		aircraft.altitudeFt = static_cast<int32_t>(lround(barometricAltitude.as<double>()));
	} else if (object["alt_geom"].is<double>()) {
		aircraft.hasAltitude = true;
		aircraft.altitudeFt = static_cast<int32_t>(lround(object["alt_geom"].as<double>()));
	}

	// Speed, track (the heading when there is no track) and vertical rate (the GPS one when there is no barometric one)
	aircraft.hasGroundSpeed = readNumber(object["gs"], aircraft.groundSpeedKt);
	aircraft.hasTrack = readNumber(object["track"], aircraft.trackDegrees) || readNumber(object["true_heading"], aircraft.trackDegrees);
	if (aircraft.hasTrack) {
		aircraft.trackDegrees = static_cast<float>(normalizeDegrees(aircraft.trackDegrees));
	}
	float verticalRate = 0;
	aircraft.hasVerticalRate = readNumber(object["baro_rate"], verticalRate) || readNumber(object["geom_rate"], verticalRate);
	if (aircraft.hasVerticalRate) {
		const float limitedRate = fmaxf(-32000.0f, fminf(32000.0f, verticalRate));
		aircraft.verticalRateFpm = static_cast<int16_t>(lroundf(limitedRate));
	}

	// Trouble: an emergency status, or its squawk
	const char *emergency = object["emergency"] | "none";
	aircraft.isEmergency = (emergency[0] != '\0' && strcmp(emergency, "none") != 0) || isEmergencySquawk(aircraft.squawk);
	return true;
}

// Read the aircraft of the flight data, keeping the nearest ones
ParseResult parseFlightData(JsonVariantConst document, const GeoPoint &center, double maxDistanceKm, bool includeGroundTraffic, Aircraft *aircraft, size_t capacity) {
	ParseResult result = { false, 0, 0 };

	// The list is "ac" in the v3 API, "aircraft" in the v2 one
	JsonArrayConst list = document["ac"].is<JsonArrayConst>() ? document["ac"].as<JsonArrayConst>() : document["aircraft"].as<JsonArrayConst>();
	if (list.isNull()) {
		return result;
	}
	result.isValid = true;

	// Keep the nearest aircraft, sorted by distance (insertion into a short list)
	for (JsonVariantConst item : list) {
		result.totalCount++;
		Aircraft candidate;
		if (!item.is<JsonObjectConst>() || !parseAircraft(item.as<JsonObjectConst>(), center, candidate)) {
			continue;
		}
		if (candidate.distanceKm > maxDistanceKm || (candidate.isOnGround && !includeGroundTraffic)) {
			continue;
		}

		// Find its place, and skip it when the list is full of nearer ones
		size_t position = result.count;
		while (position > 0 && aircraft[position - 1].distanceKm > candidate.distanceKm) {
			position--;
		}
		if (position >= capacity) {
			continue;
		}

		// Make room (the farthest drops off a full list) and insert it
		const size_t lastIndex = result.count < capacity ? result.count : capacity - 1;
		for (size_t index = lastIndex; index > position; index--) {
			aircraft[index] = aircraft[index - 1];
		}
		aircraft[position] = candidate;
		if (result.count < capacity) {
			result.count++;
		}
	}
	return result;
}
