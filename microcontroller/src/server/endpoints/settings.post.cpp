#include "../server.h"
#include <ArduinoJson.h>
#include "../../flights/flights.h"
#include "../../serial/serial.h"
#include "../../storage/storage.h"

// Save the settings of the radar (only the fields in the body change)
void setupSettingsPostRoute() {
	server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		// Read the JSON body
		JsonDocument doc;
		if (!gizmo::readJsonBody(request, data, len, index, total, doc)) {
			return;
		}

		// Refuse the body when a field is wrong, naming it
		const char *invalidField = findInvalidSettingsField(doc.as<JsonObjectConst>());
		if (invalidField != nullptr) {
			printLogfln("Invalid settings field: %s", invalidField);
			sendErrorResponse(request, 400, "invalid_settings_field", invalidField);
			return;
		}

		// Apply the fields to a copy of the current settings, and save it before using it
		const RadarSettings previous = getRadarSettings();
		RadarSettings settings = previous;
		applySettingsFromJson(doc.as<JsonObjectConst>(), settings);
		if (!saveRadarSettings(settings)) {
			sendErrorResponse(request, 500, "settings_save_failed", "Failed to save the settings");
			return;
		}
		setRadarSettings(settings);

		// Download again when the area changed: a new location forgets the aircraft of the old one
		const bool isLocationChanged = settings.hasLocation != previous.hasLocation || settings.latitude != previous.latitude || settings.longitude != previous.longitude;
		if (isLocationChanged || settings.rangeKm != previous.rangeKm || settings.showGroundTraffic != previous.showGroundTraffic) {
			requestFlightRefresh(isLocationChanged);
		}

		// Send the saved settings back
		JsonDocument response;
		writeSettingsToJson(response.to<JsonObject>(), settings);
		sendSuccessResponse(request, 200, &response);
	});

	// OPTIONS request for preflighted requests
	server.on("/api/settings", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});
}
