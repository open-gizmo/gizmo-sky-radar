#include "../server.h"
#include <ArduinoJson.h>

// Get the settings of the radar
void setupConfigGetRoute() {
	server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Send the current settings
		JsonDocument doc;
		writeSettingsToJson(doc.to<JsonObject>(), getRadarSettings());
		sendSuccessResponse(request, 200, &doc);
	});
}
