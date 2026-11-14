#include <unity.h>
#include <string.h>

#include "config/radar_settings.h"

namespace {
	// Parse a JSON body into a document (the test bodies are always valid JSON)
	JsonDocument parse(const char *json) {
		JsonDocument doc;
		deserializeJson(doc, json);
		return doc;
	}

	// The field a body is refused for (nullptr when it's valid)
	const char *findInvalidField(const char *json) {
		static JsonDocument doc;
		doc = parse(json);
		return findInvalidSettingsField(doc.as<JsonObjectConst>());
	}
}

// Nothing to set up before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Defaults ********************************/

// A new radar has no location, and shows 40 km with the sweep, labels and trails, without ground traffic
void test_starts_without_a_location() {
	const RadarSettings settings = getDefaultRadarSettings();
	TEST_ASSERT_FALSE(settings.hasLocation);
	TEST_ASSERT_EQUAL_UINT16(40, settings.rangeKm);
	TEST_ASSERT_EQUAL(UnitSystem::Aviation, settings.units);
	TEST_ASSERT_EQUAL(RadarTheme::Phosphor, settings.theme);
	TEST_ASSERT_EQUAL_UINT8(80, settings.brightness);
	TEST_ASSERT_TRUE(settings.showLabels);
	TEST_ASSERT_TRUE(settings.showTrails);
	TEST_ASSERT_TRUE(settings.showSweep);
	TEST_ASSERT_FALSE(settings.showGroundTraffic);
	TEST_ASSERT_EQUAL_STRING("", settings.locationName);
}

// Only the ranges of the list are valid
void test_knows_the_ranges() {
	TEST_ASSERT_TRUE(isValidRangeKm(5));
	TEST_ASSERT_TRUE(isValidRangeKm(150));
	TEST_ASSERT_FALSE(isValidRangeKm(0));
	TEST_ASSERT_FALSE(isValidRangeKm(41));
	TEST_ASSERT_FALSE(isValidRangeKm(200));
}

/******************************** Validation ********************************/

// A complete, valid body passes, and so does an empty one (missing fields keep their value)
void test_accepts_valid_settings() {
	TEST_ASSERT_NULL(findInvalidField("{}"));
	TEST_ASSERT_NULL(findInvalidField("{\"latitude\":45.4642,\"longitude\":9.19,\"locationName\":\"Milano\",\"rangeKm\":20,\"units\":\"metric\",\"theme\":\"amber\",\"brightness\":5,\"rotation\":3,\"showLabels\":false,\"showTrails\":false,\"showGroundTraffic\":true,\"showSweep\":false}"));
	TEST_ASSERT_NULL(findInvalidField("{\"latitude\":-90,\"longitude\":180}"));
	TEST_ASSERT_NULL(findInvalidField("{\"latitude\":0,\"longitude\":0}"));
}

// Each wrong field is named in the error
void test_names_the_wrong_field() {
	TEST_ASSERT_EQUAL_STRING("latitude", findInvalidField("{\"latitude\":91,\"longitude\":9}"));
	TEST_ASSERT_EQUAL_STRING("latitude", findInvalidField("{\"latitude\":\"45\",\"longitude\":9}"));
	TEST_ASSERT_EQUAL_STRING("longitude", findInvalidField("{\"latitude\":45,\"longitude\":-180.5}"));
	TEST_ASSERT_EQUAL_STRING("locationName", findInvalidField("{\"locationName\":\"A name much longer than the field\"}"));
	TEST_ASSERT_EQUAL_STRING("locationName", findInvalidField("{\"locationName\":12}"));
	TEST_ASSERT_EQUAL_STRING("rangeKm", findInvalidField("{\"rangeKm\":42}"));
	TEST_ASSERT_EQUAL_STRING("rangeKm", findInvalidField("{\"rangeKm\":\"40\"}"));
	TEST_ASSERT_EQUAL_STRING("units", findInvalidField("{\"units\":\"parsecs\"}"));
	TEST_ASSERT_EQUAL_STRING("theme", findInvalidField("{\"theme\":\"pink\"}"));
	TEST_ASSERT_EQUAL_STRING("brightness", findInvalidField("{\"brightness\":4}"));
	TEST_ASSERT_EQUAL_STRING("brightness", findInvalidField("{\"brightness\":101}"));
	TEST_ASSERT_EQUAL_STRING("brightness", findInvalidField("{\"brightness\":50.5}"));
	TEST_ASSERT_EQUAL_STRING("rotation", findInvalidField("{\"rotation\":4}"));
	TEST_ASSERT_EQUAL_STRING("showSweep", findInvalidField("{\"showSweep\":1}"));
}

// The coordinates come together: one alone would move the radar half way
void test_needs_both_coordinates() {
	TEST_ASSERT_EQUAL_STRING("longitude", findInvalidField("{\"latitude\":45}"));
	TEST_ASSERT_EQUAL_STRING("latitude", findInvalidField("{\"longitude\":9}"));
}

/******************************** JSON ********************************/

// The fields of a body are applied, and setting coordinates gives the radar its location
void test_applies_the_settings() {
	RadarSettings settings = getDefaultRadarSettings();
	JsonDocument doc = parse("{\"latitude\":45.4642,\"longitude\":9.19,\"locationName\":\"Milano\",\"rangeKm\":80,\"units\":\"imperial\",\"theme\":\"arctic\",\"brightness\":30,\"rotation\":2,\"showTrails\":false,\"showGroundTraffic\":true}");
	applySettingsFromJson(doc.as<JsonObjectConst>(), settings);
	TEST_ASSERT_TRUE(settings.hasLocation);
	TEST_ASSERT_EQUAL_DOUBLE(45.4642, settings.latitude);
	TEST_ASSERT_EQUAL_DOUBLE(9.19, settings.longitude);
	TEST_ASSERT_EQUAL_STRING("Milano", settings.locationName);
	TEST_ASSERT_EQUAL_UINT16(80, settings.rangeKm);
	TEST_ASSERT_EQUAL(UnitSystem::Imperial, settings.units);
	TEST_ASSERT_EQUAL(RadarTheme::Arctic, settings.theme);
	TEST_ASSERT_EQUAL_UINT8(30, settings.brightness);
	TEST_ASSERT_EQUAL_UINT8(2, settings.rotation);
	TEST_ASSERT_FALSE(settings.showTrails);
	TEST_ASSERT_TRUE(settings.showGroundTraffic);
	TEST_ASSERT_TRUE(settings.showLabels); // Not in the body: kept
}

// Missing fields keep their value, the location among them
void test_keeps_missing_fields() {
	RadarSettings settings = getDefaultRadarSettings();
	settings.hasLocation = true;
	settings.latitude = 1.5;
	settings.longitude = 2.5;
	JsonDocument doc = parse("{\"brightness\":60}");
	applySettingsFromJson(doc.as<JsonObjectConst>(), settings);
	TEST_ASSERT_TRUE(settings.hasLocation);
	TEST_ASSERT_EQUAL_DOUBLE(1.5, settings.latitude);
	TEST_ASSERT_EQUAL_UINT8(60, settings.brightness);
}

// The settings go out with names for the units and the theme, and null coordinates without a location
void test_writes_the_settings() {
	JsonDocument doc;
	writeSettingsToJson(doc.to<JsonObject>(), getDefaultRadarSettings());
	TEST_ASSERT_FALSE(doc["hasLocation"].as<bool>());
	TEST_ASSERT_TRUE(doc["latitude"].isNull());
	TEST_ASSERT_TRUE(doc["longitude"].isNull());
	TEST_ASSERT_EQUAL_STRING("aviation", doc["units"]);
	TEST_ASSERT_EQUAL_STRING("phosphor", doc["theme"]);
	TEST_ASSERT_EQUAL_INT(40, doc["rangeKm"].as<int>());

	// With a location, its coordinates
	RadarSettings settings = getDefaultRadarSettings();
	settings.hasLocation = true;
	settings.latitude = -33.8688;
	settings.longitude = 151.2093;
	settings.units = UnitSystem::Metric;
	doc.clear();
	writeSettingsToJson(doc.to<JsonObject>(), settings);
	TEST_ASSERT_EQUAL_DOUBLE(-33.8688, doc["latitude"].as<double>());
	TEST_ASSERT_EQUAL_STRING("metric", doc["units"]);
}

// What is written can be read back the same
void test_round_trips_through_json() {
	RadarSettings settings = getDefaultRadarSettings();
	settings.hasLocation = true;
	settings.latitude = 51.47;
	settings.longitude = -0.4543;
	strcpy(settings.locationName, "Heathrow");
	settings.theme = RadarTheme::Amber;
	settings.rotation = 1;
	JsonDocument doc;
	writeSettingsToJson(doc.to<JsonObject>(), settings);
	TEST_ASSERT_NULL(findInvalidSettingsField(doc.as<JsonObjectConst>()));
	RadarSettings copy = getDefaultRadarSettings();
	applySettingsFromJson(doc.as<JsonObjectConst>(), copy);
	TEST_ASSERT_EQUAL_STRING("Heathrow", copy.locationName);
	TEST_ASSERT_EQUAL(RadarTheme::Amber, copy.theme);
	TEST_ASSERT_EQUAL_UINT8(1, copy.rotation);
	TEST_ASSERT_EQUAL_DOUBLE(-0.4543, copy.longitude);
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_starts_without_a_location);
	RUN_TEST(test_knows_the_ranges);
	RUN_TEST(test_accepts_valid_settings);
	RUN_TEST(test_names_the_wrong_field);
	RUN_TEST(test_needs_both_coordinates);
	RUN_TEST(test_applies_the_settings);
	RUN_TEST(test_keeps_missing_fields);
	RUN_TEST(test_writes_the_settings);
	RUN_TEST(test_round_trips_through_json);
	return UNITY_END();
}
