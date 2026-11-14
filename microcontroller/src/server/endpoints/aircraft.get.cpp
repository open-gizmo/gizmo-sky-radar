#include "../server.h"
#include <ArduinoJson.h>
#include "../../flights/aircraft.h"

// Get the aircraft around the radar, nearest first, with their trails (the web app polls it)
void setupAircraftGetRoute() {
	server.on("/api/aircraft", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Write the aircraft straight into the response (a busy sky is a few tens of KB)
		JsonDocument doc;
		doc["errorId"] = nullptr;
		doc["errorMessage"] = nullptr;
		JsonObject data = doc["data"].to<JsonObject>();
		writeNearbyAircraft(data, MAX_AIRCRAFT, true);
		AsyncResponseStream *response = request->beginResponseStream("application/json");
		response->addHeader("Cache-Control", "no-store");
		serializeJson(doc, *response);
		request->send(response);
	});
}
