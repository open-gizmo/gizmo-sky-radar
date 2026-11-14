#include "server.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include "../flights/aircraft_json.h"
#include "../flights/flights.h"

namespace {
	// The copy of the aircraft the web server reads (in the PSRAM: too big for the internal RAM; only the web server task uses it)
	AircraftTracker *webTracker = nullptr;

	// Get the copy, allocated the first time
	AircraftTracker *getWebTracker() {
		if (webTracker == nullptr) {
			webTracker = static_cast<AircraftTracker *>(heap_caps_calloc(1, sizeof(AircraftTracker), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
		}
		return webTracker;
	}

	// Write the center of the radar (null without a location)
	void writeCenter(JsonObject result, const RadarSettings &settings) {
		if (settings.hasLocation) {
			result["center"]["latitude"] = settings.latitude;
			result["center"]["longitude"] = settings.longitude;
		} else {
			result["center"] = nullptr;
		}
	}

	// Write the state of the flight data
	void writeDataState(JsonObject result, const FlightDataState &state) {
		result["status"] = getFlightStatusName(state.status);
		if (state.hasData) {
			result["dataAgeSeconds"] = state.dataAgeSeconds;
		} else {
			result["dataAgeSeconds"] = nullptr;
		}
		result["areaTotal"] = state.areaTotal;
		if (state.lastError[0] != '\0') {
			result["error"] = JsonString(state.lastError, JsonString::Copied);
		} else {
			result["error"] = nullptr;
		}
	}
}

// Write the aircraft around the radar
void writeNearbyAircraft(JsonObject result, size_t limit, bool includeTrails) {
	const RadarSettings settings = getRadarSettings();
	AircraftTracker *tracker = getWebTracker();
	FlightDataState state;
	if (tracker == nullptr) {
		readFlightState(state);
	} else {
		readFlights(*tracker, state);
	}
	const uint32_t nowMs = millis();

	// The radar and its data
	writeCenter(result, settings);
	result["rangeKm"] = settings.rangeKm;
	writeDataState(result, state);

	// The aircraft, nearest first
	JsonArray aircraft = result["aircraft"].to<JsonArray>();
	const size_t count = tracker == nullptr ? 0 : (tracker->count < limit ? tracker->count : limit);
	for (size_t index = 0; index < count; index++) {
		writeAircraftToJson(aircraft.add<JsonObject>(), *tracker, tracker->items[index], nowMs, settings.rangeKm, includeTrails);
	}
}

// Write the status of the radar
void writeRadarStatus(JsonObject result) {
	const RadarSettings settings = getRadarSettings();
	FlightDataState state;
	readFlightState(state);

	// Its settings (location, range, units and display), then the state of its data
	writeSettingsToJson(result, settings);
	writeDataState(result, state);
	result["dataSource"] = "adsb.fi";
}
