#include "config.h"
#include <freertos/FreeRTOS.h>

namespace {
	// The current settings, until the storage loads the saved ones
	RadarSettings currentSettings = getDefaultRadarSettings();
	uint32_t settingsRevision = 0;

	// Guard the settings (shared by the main loop, the web server task and the flight data task)
	portMUX_TYPE settingsMux = portMUX_INITIALIZER_UNLOCKED;
}

// Web server shared by the Wi-Fi setup, the device API and the radar API
AsyncWebServer server(80);

// Get a copy of the current settings
RadarSettings getRadarSettings() {
	portENTER_CRITICAL(&settingsMux);
	const RadarSettings settings = currentSettings;
	portEXIT_CRITICAL(&settingsMux);
	return settings;
}

// Replace the current settings
void setRadarSettings(const RadarSettings &settings) {
	portENTER_CRITICAL(&settingsMux);
	currentSettings = settings;
	settingsRevision++;
	portEXIT_CRITICAL(&settingsMux);
}

// A number that changes every time the settings change
uint32_t getSettingsRevision() {
	portENTER_CRITICAL(&settingsMux);
	const uint32_t revision = settingsRevision;
	portEXIT_CRITICAL(&settingsMux);
	return revision;
}
