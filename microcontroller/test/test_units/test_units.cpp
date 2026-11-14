#include <unity.h>

#include "display/units.h"

namespace {
	char text[32];
}

// Nothing to set up before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

// Altitudes: flight levels high up in aviation units, feet or meters otherwise, and the ground
void test_formats_altitudes() {
	formatAltitude(text, sizeof(text), true, false, 35950, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("FL360", text);
	formatAltitude(text, sizeof(text), true, false, 12500, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("12,500 ft", text);
	formatAltitude(text, sizeof(text), true, false, 35950, UnitSystem::Imperial);
	TEST_ASSERT_EQUAL_STRING("35,950 ft", text);
	formatAltitude(text, sizeof(text), true, false, 12500, UnitSystem::Metric);
	TEST_ASSERT_EQUAL_STRING("3,810 m", text);
	formatAltitude(text, sizeof(text), true, false, 725, UnitSystem::Metric);
	TEST_ASSERT_EQUAL_STRING("221 m", text);
	formatAltitude(text, sizeof(text), false, true, 0, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("GND", text);
	formatAltitude(text, sizeof(text), false, false, 0, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("---", text);
}

// Short altitudes for the labels: hundreds of feet or meters
void test_formats_short_altitudes() {
	formatShortAltitude(text, sizeof(text), true, false, 35950, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("360", text);
	formatShortAltitude(text, sizeof(text), true, false, 4500, UnitSystem::Imperial);
	TEST_ASSERT_EQUAL_STRING("045", text);
	formatShortAltitude(text, sizeof(text), true, false, 35000, UnitSystem::Metric);
	TEST_ASSERT_EQUAL_STRING("107", text);
	formatShortAltitude(text, sizeof(text), true, false, -200, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("000", text);
	formatShortAltitude(text, sizeof(text), false, true, 0, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("GND", text);
	formatShortAltitude(text, sizeof(text), false, false, 0, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("", text);
}

// Distances, with a decimal under 10
void test_formats_distances() {
	formatDistance(text, sizeof(text), 37.04, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("20 nm", text);
	formatDistance(text, sizeof(text), 9.26, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("5.0 nm", text);
	formatDistance(text, sizeof(text), 23.4, UnitSystem::Metric);
	TEST_ASSERT_EQUAL_STRING("23 km", text);
	formatDistance(text, sizeof(text), 32.18688, UnitSystem::Imperial);
	TEST_ASSERT_EQUAL_STRING("20 mi", text);
}

// Speeds
void test_formats_speeds() {
	formatSpeed(text, sizeof(text), 450, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("450 kt", text);
	formatSpeed(text, sizeof(text), 450, UnitSystem::Metric);
	TEST_ASSERT_EQUAL_STRING("833 km/h", text);
	formatSpeed(text, sizeof(text), 450, UnitSystem::Imperial);
	TEST_ASSERT_EQUAL_STRING("518 mph", text);
}

// Ranges, without decimals when whole
void test_formats_ranges() {
	formatRange(text, sizeof(text), 40, UnitSystem::Metric);
	TEST_ASSERT_EQUAL_STRING("40 km", text);
	formatRange(text, sizeof(text), 40, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("22 nm", text);
	formatRange(text, sizeof(text), 5, UnitSystem::Aviation);
	TEST_ASSERT_EQUAL_STRING("2.7 nm", text);
	formatRange(text, sizeof(text), 10, UnitSystem::Imperial);
	TEST_ASSERT_EQUAL_STRING("6.2 mi", text);
}

// Compass points, with the halfway bearings on the right side
void test_names_compass_points() {
	TEST_ASSERT_EQUAL_STRING("N", getCompassPoint(0));
	TEST_ASSERT_EQUAL_STRING("N", getCompassPoint(359));
	TEST_ASSERT_EQUAL_STRING("NE", getCompassPoint(30));
	TEST_ASSERT_EQUAL_STRING("E", getCompassPoint(90));
	TEST_ASSERT_EQUAL_STRING("SW", getCompassPoint(225));
	TEST_ASSERT_EQUAL_STRING("NW", getCompassPoint(-45));
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_formats_altitudes);
	RUN_TEST(test_formats_short_altitudes);
	RUN_TEST(test_formats_distances);
	RUN_TEST(test_formats_speeds);
	RUN_TEST(test_formats_ranges);
	RUN_TEST(test_names_compass_points);
	return UNITY_END();
}
