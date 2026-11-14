#include <unity.h>

#include "flights/fetch_schedule.h"

namespace {
	FetchSchedule schedule;
}

// Start each test from a fresh boot
void setUp() {
	resetFetchSchedule(schedule);
}

// Nothing to clean up after each test
void tearDown() {}

// The first download starts right away, and only one runs at a time
void test_fetches_right_away() {
	TEST_ASSERT_TRUE(isFetchDue(schedule, 0));
	startFetch(schedule);
	TEST_ASSERT_FALSE(isFetchDue(schedule, 100000));
}

// After a success, the next download comes after the interval
void test_waits_the_interval_after_a_success() {
	startFetch(schedule);
	completeFetch(schedule, FetchOutcome::Success, 1000);
	TEST_ASSERT_FALSE(isFetchDue(schedule, 1000 + FETCH_INTERVAL_MS - 1));
	TEST_ASSERT_TRUE(isFetchDue(schedule, 1000 + FETCH_INTERVAL_MS));
}

// Failures in a row wait longer and longer, up to the longest wait, and a success starts over
void test_backs_off_after_failures() {
	const uint32_t expectedWaits[] = { 5000, 10000, 20000, 40000, 60000, 60000 };
	uint32_t now = 0;
	for (uint32_t expectedWait : expectedWaits) {
		startFetch(schedule);
		completeFetch(schedule, FetchOutcome::Failure, now);
		TEST_ASSERT_EQUAL_UINT32(expectedWait, schedule.waitDuration);
		now += expectedWait;
	}
	startFetch(schedule);
	completeFetch(schedule, FetchOutcome::Success, now);
	TEST_ASSERT_EQUAL_UINT8(0, schedule.failuresInRow);
	TEST_ASSERT_EQUAL_UINT32(FETCH_INTERVAL_MS, schedule.waitDuration);
}

// "Too many requests" waits at least the rate limit pause
void test_slows_down_when_rate_limited() {
	startFetch(schedule);
	completeFetch(schedule, FetchOutcome::RateLimited, 0);
	TEST_ASSERT_EQUAL_UINT32(FETCH_RATE_LIMITED_MS, schedule.waitDuration);
}

// A download asked for (a new location) starts right away, or right after the running one
void test_fetches_when_asked() {
	startFetch(schedule);
	completeFetch(schedule, FetchOutcome::Success, 1000);
	requestFetch(schedule);
	TEST_ASSERT_TRUE(isFetchDue(schedule, 1001));
	startFetch(schedule);
	requestFetch(schedule);
	TEST_ASSERT_FALSE(isFetchDue(schedule, 1002));
	completeFetch(schedule, FetchOutcome::Success, 2000);
	TEST_ASSERT_TRUE(isFetchDue(schedule, 2001));
}

// Data is stale only once a download has worked and the next ones keep failing for a minute
void test_tells_stale_data() {
	TEST_ASSERT_FALSE(isDataStale(schedule, 600000));
	startFetch(schedule);
	completeFetch(schedule, FetchOutcome::Success, 1000);
	TEST_ASSERT_FALSE(isDataStale(schedule, 1000 + DATA_STALE_AFTER_MS - 1));
	TEST_ASSERT_TRUE(isDataStale(schedule, 1000 + DATA_STALE_AFTER_MS));
	TEST_ASSERT_EQUAL_UINT32(90, getDataAgeSeconds(schedule, 91000));
}

// The waits stay right across the rollover of millis()
void test_survives_the_millis_rollover() {
	const uint32_t beforeRollover = 0xFFFFF000UL;
	startFetch(schedule);
	completeFetch(schedule, FetchOutcome::Success, beforeRollover);
	TEST_ASSERT_FALSE(isFetchDue(schedule, beforeRollover + 1000));
	TEST_ASSERT_TRUE(isFetchDue(schedule, beforeRollover + FETCH_INTERVAL_MS));
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_fetches_right_away);
	RUN_TEST(test_waits_the_interval_after_a_success);
	RUN_TEST(test_backs_off_after_failures);
	RUN_TEST(test_slows_down_when_rate_limited);
	RUN_TEST(test_fetches_when_asked);
	RUN_TEST(test_tells_stale_data);
	RUN_TEST(test_survives_the_millis_rollover);
	return UNITY_END();
}
