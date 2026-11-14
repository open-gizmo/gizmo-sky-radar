#include "radar_settings.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {
	// Names of the units and themes in the web API, in the order of their enums
	constexpr const char *UNIT_SYSTEM_NAMES[] = { "aviation", "metric", "imperial" };
	constexpr const char *RADAR_THEME_NAMES[] = { "phosphor", "amber", "arctic" };
	constexpr size_t UNIT_SYSTEM_COUNT = sizeof(UNIT_SYSTEM_NAMES) / sizeof(UNIT_SYSTEM_NAMES[0]);
	constexpr size_t RADAR_THEME_COUNT = sizeof(RADAR_THEME_NAMES) / sizeof(RADAR_THEME_NAMES[0]);

	// Find a name in a list (-1 if it isn't there)
	int findName(const char *name, const char *const *names, size_t count) {
		for (size_t index = 0; index < count; index++) {
			if (strcmp(name, names[index]) == 0) {
				return static_cast<int>(index);
			}
		}
		return -1;
	}

	// Check a number field: missing, or a number within the limits (NaN and infinity are refused)
	bool isNumberInRange(JsonVariantConst value, double minimum, double maximum) {
		if (value.isNull()) {
			return true;
		}
		if (!value.is<double>()) {
			return false;
		}
		const double number = value.as<double>();
		return isfinite(number) && number >= minimum && number <= maximum;
	}

	// Check a whole number field: missing, or an integer within the limits
	bool isIntegerInRange(JsonVariantConst value, long minimum, long maximum) {
		if (value.isNull()) {
			return true;
		}
		return value.is<long>() && value.as<long>() >= minimum && value.as<long>() <= maximum;
	}

	// Check a switch field: missing, or true or false
	bool isBooleanOrMissing(JsonVariantConst value) {
		return value.isNull() || value.is<bool>();
	}

	// Check a name field: missing, or one of the names
	bool isNameOrMissing(JsonVariantConst value, const char *const *names, size_t count) {
		return value.isNull() || (value.is<const char *>() && findName(value.as<const char *>(), names, count) >= 0);
	}

	// Copy a text into a fixed field, cut to fit
	void copyText(char *destination, const char *text, size_t size) {
		snprintf(destination, size, "%s", text == nullptr ? "" : text);
	}
}

// Settings of a new device (or after a reset): no location yet
RadarSettings getDefaultRadarSettings() {
	RadarSettings settings = {};
	settings.hasLocation = false;
	settings.latitude = 0;
	settings.longitude = 0;
	settings.locationName[0] = '\0';
	settings.rangeKm = RANGE_DEFAULT_KM;
	settings.units = UnitSystem::Aviation;
	settings.theme = RadarTheme::Phosphor;
	settings.brightness = BRIGHTNESS_DEFAULT;
	settings.rotation = 0;
	settings.showLabels = true;
	settings.showTrails = true;
	settings.showGroundTraffic = false;
	settings.showSweep = true;
	return settings;
}

// Whether a range is one the radar offers
bool isValidRangeKm(long rangeKm) {
	for (uint16_t option : RANGE_OPTIONS_KM) {
		if (rangeKm == option) {
			return true;
		}
	}
	return false;
}

// The field of the settings JSON that is wrong (nullptr when every field present is valid)
const char *findInvalidSettingsField(JsonObjectConst json) {
	// The location: both coordinates come together, and the name fits its field
	if (!isNumberInRange(json["latitude"], -90, 90)) {
		return "latitude";
	}
	if (!isNumberInRange(json["longitude"], -180, 180)) {
		return "longitude";
	}
	if (json["latitude"].isNull() != json["longitude"].isNull()) {
		return json["latitude"].isNull() ? "latitude" : "longitude";
	}
	JsonVariantConst locationName = json["locationName"];
	if (!locationName.isNull() && (!locationName.is<const char *>() || strlen(locationName.as<const char *>()) >= LOCATION_NAME_SIZE)) {
		return "locationName";
	}

	// The radar
	JsonVariantConst range = json["rangeKm"];
	if (!range.isNull() && (!range.is<long>() || !isValidRangeKm(range.as<long>()))) {
		return "rangeKm";
	}
	if (!isNameOrMissing(json["units"], UNIT_SYSTEM_NAMES, UNIT_SYSTEM_COUNT)) {
		return "units";
	}
	if (!isNameOrMissing(json["theme"], RADAR_THEME_NAMES, RADAR_THEME_COUNT)) {
		return "theme";
	}
	if (!isIntegerInRange(json["brightness"], BRIGHTNESS_MIN, BRIGHTNESS_MAX)) {
		return "brightness";
	}
	if (!isIntegerInRange(json["rotation"], 0, ROTATION_MAX)) {
		return "rotation";
	}

	// The switches
	constexpr const char *SWITCH_FIELDS[] = { "showLabels", "showTrails", "showGroundTraffic", "showSweep" };
	for (const char *field : SWITCH_FIELDS) {
		if (!isBooleanOrMissing(json[field])) {
			return field;
		}
	}

	// Every field present is valid
	return nullptr;
}

// Apply the fields present in a validated settings JSON
void applySettingsFromJson(JsonObjectConst json, RadarSettings &settings) {
	// The location (validation makes sure both coordinates come together)
	if (!json["latitude"].isNull() && !json["longitude"].isNull()) {
		settings.latitude = json["latitude"].as<double>();
		settings.longitude = json["longitude"].as<double>();
		settings.hasLocation = true;
	}
	if (!json["locationName"].isNull()) {
		copyText(settings.locationName, json["locationName"].as<const char *>(), sizeof(settings.locationName));
	}

	// The radar
	if (!json["rangeKm"].isNull()) {
		settings.rangeKm = json["rangeKm"].as<uint16_t>();
	}
	if (!json["units"].isNull()) {
		settings.units = static_cast<UnitSystem>(findName(json["units"].as<const char *>(), UNIT_SYSTEM_NAMES, UNIT_SYSTEM_COUNT));
	}
	if (!json["theme"].isNull()) {
		settings.theme = static_cast<RadarTheme>(findName(json["theme"].as<const char *>(), RADAR_THEME_NAMES, RADAR_THEME_COUNT));
	}
	if (!json["brightness"].isNull()) {
		settings.brightness = json["brightness"].as<uint8_t>();
	}
	if (!json["rotation"].isNull()) {
		settings.rotation = json["rotation"].as<uint8_t>();
	}

	// The switches
	if (!json["showLabels"].isNull()) {
		settings.showLabels = json["showLabels"].as<bool>();
	}
	if (!json["showTrails"].isNull()) {
		settings.showTrails = json["showTrails"].as<bool>();
	}
	if (!json["showGroundTraffic"].isNull()) {
		settings.showGroundTraffic = json["showGroundTraffic"].as<bool>();
	}
	if (!json["showSweep"].isNull()) {
		settings.showSweep = json["showSweep"].as<bool>();
	}
}

// Write the settings to a JSON object, as the web API sends them
void writeSettingsToJson(JsonObject json, const RadarSettings &settings) {
	// The location (null coordinates until the user sets one)
	json["hasLocation"] = settings.hasLocation;
	if (settings.hasLocation) {
		json["latitude"] = settings.latitude;
		json["longitude"] = settings.longitude;
	} else {
		json["latitude"] = nullptr;
		json["longitude"] = nullptr;
	}
	json["locationName"] = JsonString(settings.locationName, JsonString::Copied);

	// The radar
	json["rangeKm"] = settings.rangeKm;
	json["units"] = getUnitSystemName(settings.units);
	json["theme"] = getRadarThemeName(settings.theme);
	json["brightness"] = settings.brightness;
	json["rotation"] = settings.rotation;

	// The switches
	json["showLabels"] = settings.showLabels;
	json["showTrails"] = settings.showTrails;
	json["showGroundTraffic"] = settings.showGroundTraffic;
	json["showSweep"] = settings.showSweep;
}

// Name of a unit system in the web API
const char *getUnitSystemName(UnitSystem units) {
	const size_t index = static_cast<size_t>(units);
	return index < UNIT_SYSTEM_COUNT ? UNIT_SYSTEM_NAMES[index] : UNIT_SYSTEM_NAMES[0];
}

// Name of a theme in the web API
const char *getRadarThemeName(RadarTheme theme) {
	const size_t index = static_cast<size_t>(theme);
	return index < RADAR_THEME_COUNT ? RADAR_THEME_NAMES[index] : RADAR_THEME_NAMES[0];
}
