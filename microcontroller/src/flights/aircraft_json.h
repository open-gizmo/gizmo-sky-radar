#ifndef AIRCRAFT_JSON_H
#define AIRCRAFT_JSON_H

#include <ArduinoJson.h>
#include "aircraft_tracker.h"

// The aircraft as the web API sends them, kept free of hardware headers so the native tests can use it

// Write one aircraft: its identity, its last position and how old it is now, its flight data, and its place from the radar (inRange when inside the range the display shows)
void writeAircraftToJson(JsonObject json, const AircraftTracker &tracker, const TrackedAircraft &tracked, uint32_t nowMs, uint16_t rangeKm, bool includeTrail);

#endif
