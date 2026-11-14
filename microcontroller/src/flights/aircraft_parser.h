#ifndef AIRCRAFT_PARSER_H
#define AIRCRAFT_PARSER_H

#include <ArduinoJson.h>
#include "aircraft.h"

// Reading the flight data of adsb.fi (the readsb JSON format), kept free of hardware headers so the native tests can use it

// The URL that asks adsb.fi for the aircraft around a point, within a radius in nautical miles (written into url, false if it doesn't fit)
bool buildFlightDataUrl(char *url, size_t urlSize, const GeoPoint &center, uint16_t radiusNm);

// The radius to ask for, in nautical miles, for a radar range: a bit more than the range, so the aircraft just outside show on the rim (at least 1, at most 250, the most adsb.fi gives)
uint16_t getQueryRadiusNm(uint16_t rangeKm);

// Keep only the fields the radar reads, so a busy sky fits in memory (pass it to deserializeJson with DeserializationOption::Filter)
void buildFlightDataFilter(JsonDocument &filter);

// What reading the flight data found
struct ParseResult {
	bool isValid; // False when the JSON isn't flight data (no aircraft list)
	size_t count; // Aircraft written, nearest first
	size_t totalCount; // Aircraft in the data, before the filters
};

// Read the aircraft of the flight data (the whole document, "ac" or "aircraft" list), keeping the nearest ones within maxDistanceKm, with a position, nearest first (ground traffic only when asked)
ParseResult parseFlightData(JsonVariantConst document, const GeoPoint &center, double maxDistanceKm, bool includeGroundTraffic, Aircraft *aircraft, size_t capacity);

// Read one aircraft object (false when it has no position)
bool parseAircraft(JsonObjectConst object, const GeoPoint &center, Aircraft &aircraft);

#endif
