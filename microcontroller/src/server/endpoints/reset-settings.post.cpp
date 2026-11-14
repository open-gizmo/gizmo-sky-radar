#include "../server.h"
#include <ArduinoJson.h>
#include <gizmo/device.h>
#include <gizmo/wifi_setup.h>
#include "../../storage/storage.h"

// Forget the settings and the saved network, then restart into the setup hotspot (like the factory reset of the BOOT button)
void setupResetSettingsPostRoute() {
	// POST, so other pages can't trigger it with a simple link or image
	server.on("/api/reset-settings", HTTP_POST, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Clear the settings and the saved network
		const bool isSettingsCleared = clearSavedRadarSettings();
		const bool isWifiCleared = gizmo::clearWifiCredentials();
		if (!isSettingsCleared || !isWifiCleared) {
			sendErrorResponse(request, 500, "clear_settings_failed", "Failed to clear the settings");
			return;
		}

		// Answer, then restart from the main loop (so the answer leaves before the reboot)
		sendSuccessResponse(request);
		gizmo::scheduleRestart();
	});

	// OPTIONS request for preflighted requests
	server.on("/api/reset-settings", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});
}
