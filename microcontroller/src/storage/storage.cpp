#include "storage.h"
#include <Preferences.h>
#include <string.h>
#include "../config/config.h"
#include "../serial/serial.h"

namespace {
	// NVS namespace (the project name, which fits the 15 characters of a namespace) and keys (15 characters at most); the Wi-Fi lives in its own namespace, kept by gizmo-common
	constexpr char STORAGE_NAMESPACE[] = "gizmo-sky-radar";
	constexpr char HAS_LOCATION_KEY[] = "hasLocation";
	constexpr char LATITUDE_KEY[] = "latitude";
	constexpr char LONGITUDE_KEY[] = "longitude";
	constexpr char LOCATION_NAME_KEY[] = "locationName";
	constexpr char RANGE_KEY[] = "rangeKm";
	constexpr char UNITS_KEY[] = "units";
	constexpr char THEME_KEY[] = "theme";
	constexpr char BRIGHTNESS_KEY[] = "brightness";
	constexpr char ROTATION_KEY[] = "rotation";
	constexpr char LABELS_KEY[] = "showLabels";
	constexpr char TRAILS_KEY[] = "showTrails";
	constexpr char GROUND_KEY[] = "showGround";
	constexpr char SWEEP_KEY[] = "showSweep";

	// Read the saved settings on top of the defaults (missing keys keep their default, and values out of range are never used)
	RadarSettings loadSettings(Preferences &preferences, const RadarSettings &defaults) {
		RadarSettings settings = defaults;

		// Location
		const double latitude = preferences.getDouble(LATITUDE_KEY, 0);
		const double longitude = preferences.getDouble(LONGITUDE_KEY, 0);
		if (preferences.getBool(HAS_LOCATION_KEY, false) && latitude >= -90 && latitude <= 90 && longitude >= -180 && longitude <= 180) {
			settings.hasLocation = true;
			settings.latitude = latitude;
			settings.longitude = longitude;
		}
		if (preferences.isKey(LOCATION_NAME_KEY)) {
			preferences.getString(LOCATION_NAME_KEY, settings.locationName, sizeof(settings.locationName));
		}

		// Radar
		const uint16_t rangeKm = preferences.getUShort(RANGE_KEY, defaults.rangeKm);
		settings.rangeKm = isValidRangeKm(rangeKm) ? rangeKm : defaults.rangeKm;
		const uint8_t units = preferences.getUChar(UNITS_KEY, static_cast<uint8_t>(defaults.units));
		settings.units = units <= static_cast<uint8_t>(UnitSystem::Imperial) ? static_cast<UnitSystem>(units) : defaults.units;
		const uint8_t theme = preferences.getUChar(THEME_KEY, static_cast<uint8_t>(defaults.theme));
		settings.theme = theme <= static_cast<uint8_t>(RadarTheme::Arctic) ? static_cast<RadarTheme>(theme) : defaults.theme;
		const uint8_t brightness = preferences.getUChar(BRIGHTNESS_KEY, defaults.brightness);
		settings.brightness = brightness >= BRIGHTNESS_MIN && brightness <= BRIGHTNESS_MAX ? brightness : defaults.brightness;
		const uint8_t rotation = preferences.getUChar(ROTATION_KEY, defaults.rotation);
		settings.rotation = rotation <= ROTATION_MAX ? rotation : defaults.rotation;

		// Switches
		settings.showLabels = preferences.getBool(LABELS_KEY, defaults.showLabels);
		settings.showTrails = preferences.getBool(TRAILS_KEY, defaults.showTrails);
		settings.showGroundTraffic = preferences.getBool(GROUND_KEY, defaults.showGroundTraffic);
		settings.showSweep = preferences.getBool(SWEEP_KEY, defaults.showSweep);
		return settings;
	}
}

// Load the saved settings into the current ones
void setupStorage() {
	Preferences preferences;

	// A new device has nothing saved yet: keep the defaults
	if (!preferences.begin(STORAGE_NAMESPACE, true)) {
		printLogln("No saved settings, using the defaults");
		return;
	}

	// Apply the saved settings
	setRadarSettings(loadSettings(preferences, getDefaultRadarSettings()));
	preferences.end();
	printLogln("Settings loaded");
}

// Save the settings (NVS skips the values that didn't change)
bool saveRadarSettings(const RadarSettings &settings) {
	Preferences preferences;
	if (!preferences.begin(STORAGE_NAMESPACE, false)) {
		return false;
	}

	// Write every setting, checking each write (an empty name is written as an empty string, which putString counts as 0 bytes)
	bool isSaved = true;
	isSaved &= preferences.putBool(HAS_LOCATION_KEY, settings.hasLocation) > 0;
	isSaved &= preferences.putDouble(LATITUDE_KEY, settings.latitude) > 0;
	isSaved &= preferences.putDouble(LONGITUDE_KEY, settings.longitude) > 0;
	isSaved &= preferences.putString(LOCATION_NAME_KEY, settings.locationName) == strlen(settings.locationName);
	isSaved &= preferences.putUShort(RANGE_KEY, settings.rangeKm) > 0;
	isSaved &= preferences.putUChar(UNITS_KEY, static_cast<uint8_t>(settings.units)) > 0;
	isSaved &= preferences.putUChar(THEME_KEY, static_cast<uint8_t>(settings.theme)) > 0;
	isSaved &= preferences.putUChar(BRIGHTNESS_KEY, settings.brightness) > 0;
	isSaved &= preferences.putUChar(ROTATION_KEY, settings.rotation) > 0;
	isSaved &= preferences.putBool(LABELS_KEY, settings.showLabels) > 0;
	isSaved &= preferences.putBool(TRAILS_KEY, settings.showTrails) > 0;
	isSaved &= preferences.putBool(GROUND_KEY, settings.showGroundTraffic) > 0;
	isSaved &= preferences.putBool(SWEEP_KEY, settings.showSweep) > 0;
	preferences.end();

	// Log the result
	printLogln(isSaved ? "Settings saved" : "Failed to save the settings");
	return isSaved;
}

// Forget the saved settings
bool clearSavedRadarSettings() {
	Preferences preferences;
	if (!preferences.begin(STORAGE_NAMESPACE, false)) {
		return false;
	}
	const bool isCleared = preferences.clear();
	preferences.end();
	return isCleared;
}
