#include <unity.h>
#include <math.h>
#include <string.h>

#include "flights/aircraft_json.h"
#include "flights/aircraft_parser.h"
#include "flights/aircraft_tracker.h"
#include "flights/geo.h"

namespace {
	// The center of the test radar: Milan's cathedral
	constexpr GeoPoint CENTER = { 45.4642, 9.1900 };

	// A trimmed sample of adsb.fi v3 data around Milan: a jet with a padded callsign, a plane on the ground, one without a position, one with an emergency squawk, one without a callsign, and a far one
	constexpr const char *SAMPLE = R"({"ac":[
		{"hex":"4b1819","flight":"SWR2KT  ","r":"HB-JDH","t":"A20N","desc":"AIRBUS A-320neo","alt_baro":35950,"alt_geom":36500,"gs":459.9,"track":19.7,"baro_rate":-64,"squawk":"1000","emergency":"none","category":"A3","lat":45.80,"lon":9.40,"seen_pos":1.5},
		{"hex":"3c6917","flight":"CFG4VR  ","r":"D-AJHW","t":"E190","alt_baro":"ground","gs":12.1,"true_heading":166.8,"category":"A3","lat":45.62,"lon":8.72},
		{"hex":"3009c3","flight":"IIZAL","alt_baro":1800,"gs":145.0},
		{"hex":"4d251d","flight":"RYR55YP ","r":"9H-QFA","t":"B738","alt_baro":8525,"gs":255.4,"track":222.94,"geom_rate":-1200,"squawk":"7700","lat":45.50,"lon":9.20},
		{"hex":"505e7d","alt_geom":1700,"gs":153.4,"track":257.96,"lat":45.40,"lon":9.10},
		{"hex":"0100a3","flight":"MSR532","alt_baro":3575,"gs":228.4,"track":3.51,"lat":46.40,"lon":9.19}
	],"msg":"No error","now":1794664450001,"total":6})";

	// Aircraft read from the sample
	Aircraft aircraft[MAX_AIRCRAFT];
	AircraftTracker tracker;

	// Read the sample into the list, within a radius
	ParseResult parseSample(double maxDistanceKm, bool includeGroundTraffic, size_t capacity = MAX_AIRCRAFT) {
		JsonDocument doc;
		deserializeJson(doc, SAMPLE);
		return parseFlightData(doc.as<JsonVariantConst>(), CENTER, maxDistanceKm, includeGroundTraffic, aircraft, capacity);
	}

	// An aircraft at a position, flying a track at a speed
	Aircraft makeAircraft(const char *hex, GeoPoint position, float trackDegrees, float speedKt) {
		Aircraft result = {};
		strcpy(result.hex, hex);
		result.position = position;
		result.hasTrack = true;
		result.trackDegrees = trackDegrees;
		result.hasGroundSpeed = true;
		result.groundSpeedKt = speedKt;
		result.distanceKm = getDistanceKm(CENTER, position);
		return result;
	}
}

// Start each test with an empty tracker
void setUp() {
	resetTracker(tracker);
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Geography ********************************/

// Known distances and bearings: one degree of latitude is about 111 km, due north
void test_measures_distances_and_bearings() {
	const GeoPoint north = { CENTER.latitude + 1, CENTER.longitude };
	TEST_ASSERT_DOUBLE_WITHIN(0.2, 111.2, getDistanceKm(CENTER, north));
	TEST_ASSERT_DOUBLE_WITHIN(0.01, 0, getBearingDegrees(CENTER, north));
	TEST_ASSERT_DOUBLE_WITHIN(0.5, 270, getBearingDegrees(CENTER, { CENTER.latitude, CENTER.longitude - 0.5 }));

	// Milan to Rome, about 477 km to the south-east
	const GeoPoint rome = { 41.9028, 12.4964 };
	TEST_ASSERT_DOUBLE_WITHIN(3, 477, getDistanceKm(CENTER, rome));
	TEST_ASSERT_DOUBLE_WITHIN(2, 145, getBearingDegrees(CENTER, rome));
}

// Moving along a bearing lands at that distance and bearing
void test_moves_a_point_along_a_bearing() {
	const GeoPoint moved = movePoint(CENTER, 60, 25);
	TEST_ASSERT_DOUBLE_WITHIN(0.01, 25, getDistanceKm(CENTER, moved));
	TEST_ASSERT_DOUBLE_WITHIN(0.1, 60, getBearingDegrees(CENTER, moved));

	// Across the date line, the longitude stays within -180 to 180
	const GeoPoint crossed = movePoint({ 0, 179.99 }, 90, 10);
	TEST_ASSERT_TRUE(crossed.longitude < -179.9);
}

// Angles are brought to 0 to 360
void test_normalizes_angles() {
	TEST_ASSERT_DOUBLE_WITHIN(0.001, 350, normalizeDegrees(-10));
	TEST_ASSERT_DOUBLE_WITHIN(0.001, 10, normalizeDegrees(370));
	TEST_ASSERT_DOUBLE_WITHIN(0.001, 0, normalizeDegrees(360));
}

/******************************** Parser ********************************/

// The URL asks adsb.fi's v3 API for a radius in nautical miles, a bit wider than the range
void test_builds_the_url() {
	char url[128];
	TEST_ASSERT_TRUE(buildFlightDataUrl(url, sizeof(url), CENTER, 29));
	TEST_ASSERT_EQUAL_STRING("https://opendata.adsb.fi/api/v3/lat/45.46420/lon/9.19000/dist/29", url);
	TEST_ASSERT_FALSE(buildFlightDataUrl(url, 20, CENTER, 29));
	TEST_ASSERT_EQUAL_UINT16(29, getQueryRadiusNm(40)); // 40 km x 1.3 = 28.1 nm, rounded up
	TEST_ASSERT_EQUAL_UINT16(4, getQueryRadiusNm(5));
	TEST_ASSERT_EQUAL_UINT16(106, getQueryRadiusNm(150));
}

// The aircraft come nearest first, with their fields read and the callsigns trimmed; the one without a position is left out
void test_reads_the_aircraft_nearest_first() {
	const ParseResult result = parseSample(200, true);
	TEST_ASSERT_TRUE(result.isValid);
	TEST_ASSERT_EQUAL_UINT(6, result.totalCount);
	TEST_ASSERT_EQUAL_UINT(5, result.count);
	for (size_t index = 1; index < result.count; index++) {
		TEST_ASSERT_TRUE(aircraft[index - 1].distanceKm <= aircraft[index].distanceKm);
	}
	TEST_ASSERT_EQUAL_STRING("4d251d", aircraft[0].hex);

	// The jet, with every field
	const Aircraft *jet = nullptr;
	for (size_t index = 0; index < result.count; index++) {
		if (strcmp(aircraft[index].hex, "4b1819") == 0) {
			jet = &aircraft[index];
		}
	}
	TEST_ASSERT_NOT_NULL(jet);
	TEST_ASSERT_EQUAL_STRING("SWR2KT", jet->callsign);
	TEST_ASSERT_EQUAL_STRING("HB-JDH", jet->registration);
	TEST_ASSERT_EQUAL_STRING("A20N", jet->typeCode);
	TEST_ASSERT_EQUAL_STRING("AIRBUS A-320neo", jet->description);
	TEST_ASSERT_TRUE(jet->hasAltitude);
	TEST_ASSERT_EQUAL_INT32(35950, jet->altitudeFt); // The barometric altitude, not the GPS one
	TEST_ASSERT_FLOAT_WITHIN(0.01, 459.9, jet->groundSpeedKt);
	TEST_ASSERT_FLOAT_WITHIN(0.01, 19.7, jet->trackDegrees);
	TEST_ASSERT_EQUAL_INT16(-64, jet->verticalRateFpm);
	TEST_ASSERT_FLOAT_WITHIN(0.01, 1.5, jet->positionAgeSeconds);
	TEST_ASSERT_FALSE(jet->isEmergency);
	TEST_ASSERT_FALSE(jet->isOnGround);
	TEST_ASSERT_DOUBLE_WITHIN(0.5, getDistanceKm(CENTER, { 45.80, 9.40 }), jet->distanceKm);
}

// The fallbacks: "ground" altitude, the heading for a missing track, the GPS rate and altitude, the 7700 squawk
void test_reads_the_fallback_fields() {
	const ParseResult result = parseSample(200, true);
	for (size_t index = 0; index < result.count; index++) {
		const Aircraft &item = aircraft[index];
		if (strcmp(item.hex, "3c6917") == 0) {
			TEST_ASSERT_TRUE(item.isOnGround);
			TEST_ASSERT_FALSE(item.hasAltitude);
			TEST_ASSERT_FLOAT_WITHIN(0.01, 166.8, item.trackDegrees);
		}
		if (strcmp(item.hex, "4d251d") == 0) {
			TEST_ASSERT_TRUE(item.isEmergency);
			TEST_ASSERT_EQUAL_INT16(-1200, item.verticalRateFpm);
		}
		if (strcmp(item.hex, "505e7d") == 0) {
			TEST_ASSERT_EQUAL_INT32(1700, item.altitudeFt);
			TEST_ASSERT_EQUAL_STRING("505e7d", getAircraftDisplayName(item)); // No callsign, no registration
		}
	}
}

// Far aircraft and ground traffic are left out when asked, and a full list keeps the nearest
void test_filters_the_aircraft() {
	TEST_ASSERT_EQUAL_UINT(3, parseSample(60, false).count); // The far one and the one on the ground are out
	const ParseResult result = parseSample(200, true, 2);
	TEST_ASSERT_EQUAL_UINT(2, result.count);
	TEST_ASSERT_EQUAL_STRING("4d251d", aircraft[0].hex);
	TEST_ASSERT_EQUAL_STRING("505e7d", aircraft[1].hex);
}

// Data that isn't a flight list is refused, and the v2 list ("aircraft") is read too
void test_reads_both_api_versions() {
	JsonDocument doc;
	deserializeJson(doc, "{\"error\":\"rate limited\"}");
	TEST_ASSERT_FALSE(parseFlightData(doc.as<JsonVariantConst>(), CENTER, 100, true, aircraft, MAX_AIRCRAFT).isValid);
	deserializeJson(doc, "{\"aircraft\":[{\"hex\":\"abc123\",\"lat\":45.5,\"lon\":9.2}],\"now\":1794664450.0}");
	const ParseResult result = parseFlightData(doc.as<JsonVariantConst>(), CENTER, 100, true, aircraft, MAX_AIRCRAFT);
	TEST_ASSERT_TRUE(result.isValid);
	TEST_ASSERT_EQUAL_UINT(1, result.count);
}

// The filter keeps what the parser reads, and drops the rest
void test_filters_the_json() {
	JsonDocument filter;
	buildFlightDataFilter(filter);
	JsonDocument doc;
	deserializeJson(doc, SAMPLE, DeserializationOption::Filter(filter));
	TEST_ASSERT_EQUAL_STRING("SWR2KT  ", doc["ac"][0]["flight"]);
	TEST_ASSERT_TRUE(doc["ac"][0]["alt_geom"].is<int>());
	TEST_ASSERT_TRUE(doc["msg"].isNull());
}

/******************************** Tracker ********************************/

// A known aircraft keeps its trail across downloads, a new one starts its own
void test_keeps_the_trails() {
	Aircraft first = makeAircraft("aaaaaa", { 45.50, 9.20 }, 90, 300);
	updateTracker(tracker, &first, 1, 1000);
	first.position = { 45.50, 9.21 };
	updateTracker(tracker, &first, 1, 6000);
	first.position = { 45.50, 9.22 };
	updateTracker(tracker, &first, 1, 11000);
	TEST_ASSERT_EQUAL_UINT(1, tracker.count);
	TEST_ASSERT_EQUAL_UINT8(3, tracker.items[0].trailCount);
	TEST_ASSERT_DOUBLE_WITHIN(0.0001, 9.20, tracker.items[0].trail[0].longitude);

	// A position that barely moved isn't added
	updateTracker(tracker, &first, 1, 16000);
	TEST_ASSERT_EQUAL_UINT8(3, tracker.items[0].trailCount);
}

// The trail keeps its last points only
void test_caps_the_trails() {
	Aircraft item = makeAircraft("aaaaaa", { 45.50, 9.00 }, 90, 300);
	for (int step = 0; step < 20; step++) {
		item.position.longitude = 9.00 + step * 0.01;
		updateTracker(tracker, &item, 1, 1000 + step * 5000);
	}
	TEST_ASSERT_EQUAL_UINT8(TRAIL_LENGTH, tracker.items[0].trailCount);
	TEST_ASSERT_DOUBLE_WITHIN(0.0001, 9.19, tracker.items[0].trail[TRAIL_LENGTH - 1].longitude);
}

// An aircraft missing from a download stays a while, then goes
void test_keeps_missing_aircraft_a_while() {
	Aircraft pair[2] = { makeAircraft("aaaaaa", { 45.50, 9.20 }, 90, 300), makeAircraft("bbbbbb", { 45.60, 9.20 }, 90, 300) };
	updateTracker(tracker, pair, 2, 1000);
	updateTracker(tracker, pair, 1, 6000);
	TEST_ASSERT_EQUAL_UINT(2, tracker.count);
	TEST_ASSERT_TRUE(isAircraftMissing(tracker, tracker.items[1]));
	TEST_ASSERT_FALSE(isAircraftMissing(tracker, tracker.items[0]));
	updateTracker(tracker, pair, 1, 1000 + AIRCRAFT_MISSING_TIMEOUT_MS);
	TEST_ASSERT_EQUAL_UINT(1, tracker.count);
}

// A missing aircraft keeps its place by distance
void test_sorts_missing_aircraft_by_distance() {
	Aircraft trio[3] = { makeAircraft("aaaaaa", { 45.47, 9.19 }, 90, 300), makeAircraft("bbbbbb", { 45.50, 9.19 }, 90, 300), makeAircraft("cccccc", { 45.60, 9.19 }, 90, 300) };
	updateTracker(tracker, trio, 3, 1000);
	Aircraft remaining[2] = { trio[0], trio[2] };
	updateTracker(tracker, remaining, 2, 6000);
	TEST_ASSERT_EQUAL_STRING("aaaaaa", tracker.items[0].aircraft.hex);
	TEST_ASSERT_EQUAL_STRING("bbbbbb", tracker.items[1].aircraft.hex);
	TEST_ASSERT_EQUAL_STRING("cccccc", tracker.items[2].aircraft.hex);
}

// Between downloads, an aircraft moves along its track at its speed, from when its position was true
void test_estimates_the_position() {
	Aircraft item = makeAircraft("aaaaaa", { 45.50, 9.20 }, 90, 360); // 360 kt is 0.1852 km/s
	item.positionAgeSeconds = 2;
	updateTracker(tracker, &item, 1, 10000);
	const GeoPoint now = getEstimatedPosition(tracker.items[0], 18000); // 10 s after the position
	TEST_ASSERT_DOUBLE_WITHIN(0.01, 1.852, getDistanceKm(item.position, now));
	TEST_ASSERT_DOUBLE_WITHIN(0.5, 90, getBearingDegrees(item.position, now));

	// Capped far from the last position
	const GeoPoint later = getEstimatedPosition(tracker.items[0], 8000 + 600000);
	TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.1852 * MAX_DEAD_RECKONING_MS / 1000, getDistanceKm(item.position, later));
}

// An aircraft on the ground or without a course stays where it was
void test_keeps_still_aircraft_in_place() {
	Aircraft item = makeAircraft("aaaaaa", { 45.50, 9.20 }, 90, 20);
	item.isOnGround = true;
	updateTracker(tracker, &item, 1, 1000);
	TEST_ASSERT_DOUBLE_WITHIN(0.000001, 9.20, getEstimatedPosition(tracker.items[0], 20000).longitude);
	item.isOnGround = false;
	item.hasTrack = false;
	updateTracker(tracker, &item, 1, 2000);
	TEST_ASSERT_DOUBLE_WITHIN(0.000001, 9.20, getEstimatedPosition(tracker.items[0], 20000).longitude);
}

/******************************** JSON ********************************/

// An aircraft goes out with its fields, null for what it doesn't send, how old its position is now, and its trail
void test_writes_the_aircraft_json() {
	Aircraft item = makeAircraft("4b1819", { 45.50, 9.20 }, 20, 455);
	strcpy(item.callsign, "SWR2KT");
	item.hasAltitude = true;
	item.altitudeFt = 35950;
	item.positionAgeSeconds = 1.5f;
	updateTracker(tracker, &item, 1, 10000);
	JsonDocument doc;
	writeAircraftToJson(doc.to<JsonObject>(), tracker, tracker.items[0], 12000, 40, true);
	TEST_ASSERT_EQUAL_STRING("SWR2KT", doc["callsign"]);
	TEST_ASSERT_TRUE(doc["registration"].isNull());
	TEST_ASSERT_EQUAL_INT(35950, doc["altitudeFt"].as<int>());
	TEST_ASSERT_TRUE(doc["verticalRateFpm"].isNull());
	TEST_ASSERT_DOUBLE_WITHIN(0.01, 3.5, doc["positionAgeSeconds"].as<double>());
	TEST_ASSERT_TRUE(doc["inRange"].as<bool>());
	TEST_ASSERT_FALSE(doc["missing"].as<bool>());
	TEST_ASSERT_EQUAL_UINT(1, doc["trail"].size());
	TEST_ASSERT_DOUBLE_WITHIN(0.00001, 9.2, doc["trail"][0][1].as<double>());

	// Without the trail, for the agents
	doc.clear();
	writeAircraftToJson(doc.to<JsonObject>(), tracker, tracker.items[0], 12000, 2, false);
	TEST_ASSERT_TRUE(doc["trail"].isNull());
	TEST_ASSERT_FALSE(doc["inRange"].as<bool>());
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_measures_distances_and_bearings);
	RUN_TEST(test_moves_a_point_along_a_bearing);
	RUN_TEST(test_normalizes_angles);
	RUN_TEST(test_builds_the_url);
	RUN_TEST(test_reads_the_aircraft_nearest_first);
	RUN_TEST(test_reads_the_fallback_fields);
	RUN_TEST(test_filters_the_aircraft);
	RUN_TEST(test_reads_both_api_versions);
	RUN_TEST(test_filters_the_json);
	RUN_TEST(test_keeps_the_trails);
	RUN_TEST(test_caps_the_trails);
	RUN_TEST(test_keeps_missing_aircraft_a_while);
	RUN_TEST(test_sorts_missing_aircraft_by_distance);
	RUN_TEST(test_estimates_the_position);
	RUN_TEST(test_keeps_still_aircraft_in_place);
	RUN_TEST(test_writes_the_aircraft_json);
	return UNITY_END();
}
