#ifndef RADAR_SETTINGS_H
#define RADAR_SETTINGS_H

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

// The settings of the radar, their limits and their JSON in the web API, kept free of hardware headers so the native tests can use them

// Units of the numbers on the display and in the web app
enum class UnitSystem : uint8_t {
	Aviation = 0, // Nautical miles, feet and knots, like pilots and controllers
	Metric = 1, // Kilometers, meters and km/h
	Imperial = 2 // Miles, feet and mph
};

// Color theme of the radar screen
enum class RadarTheme : uint8_t {
	Phosphor = 0, // Green, like an old radar tube
	Amber = 1, // Orange, like an amber CRT
	Arctic = 2 // Cyan and white on deep blue
};

// Ranges the radar can show (its outer ring), in kilometers, smallest to largest
constexpr uint16_t RANGE_OPTIONS_KM[] = { 5, 10, 20, 40, 80, 150 };
constexpr size_t RANGE_OPTION_COUNT = sizeof(RANGE_OPTIONS_KM) / sizeof(RANGE_OPTIONS_KM[0]);
constexpr uint16_t RANGE_DEFAULT_KM = 40;

// Backlight brightness, in percent
constexpr uint8_t BRIGHTNESS_MIN = 5; // Never fully dark, so the radar always looks on
constexpr uint8_t BRIGHTNESS_MAX = 100;
constexpr uint8_t BRIGHTNESS_DEFAULT = 80;

// Rotation of the picture, in quarter turns clockwise (to match how the display sits in its enclosure)
constexpr uint8_t ROTATION_MAX = 3;

// Size of the name of the location, shown on the boot screen and in the web app (up to 24 bytes, plus the terminator)
constexpr size_t LOCATION_NAME_SIZE = 25;

// The settings of the radar
struct RadarSettings {
	bool hasLocation; // False until the user sets one: the radar has no center without it
	double latitude; // Degrees, -90 to 90
	double longitude; // Degrees, -180 to 180
	char locationName[LOCATION_NAME_SIZE]; // Optional name of the place, for example "Home"
	uint16_t rangeKm; // Radius of the outer ring, one of RANGE_OPTIONS_KM
	UnitSystem units;
	RadarTheme theme;
	uint8_t brightness; // Percent, BRIGHTNESS_MIN to BRIGHTNESS_MAX
	uint8_t rotation; // Quarter turns clockwise, 0 to ROTATION_MAX
	bool showLabels; // Callsign and altitude next to each aircraft
	bool showTrails; // The path each aircraft flew since it appeared
	bool showGroundTraffic; // Aircraft taxiing or parked at an airport
	bool showSweep; // The rotating beam of the radar
};

// Settings of a new device (or after a reset): no location yet
RadarSettings getDefaultRadarSettings();

// Whether a range is one the radar offers
bool isValidRangeKm(long rangeKm);

// The field of the settings JSON that is wrong (nullptr when every field present is valid; missing fields are fine, they keep their value)
const char *findInvalidSettingsField(JsonObjectConst json);

// Apply the fields present in a validated settings JSON (missing fields keep their current value)
void applySettingsFromJson(JsonObjectConst json, RadarSettings &settings);

// Write the settings to a JSON object, as the web API sends them
void writeSettingsToJson(JsonObject json, const RadarSettings &settings);

// Names of the units and themes in the web API ("aviation", "phosphor"...)
const char *getUnitSystemName(UnitSystem units);
const char *getRadarThemeName(RadarTheme theme);

#endif
