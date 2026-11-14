#include "fetch_schedule.h"

// Start with a download right away
void resetFetchSchedule(FetchSchedule &schedule) {
	schedule.isFetchInProgress = false;
	schedule.isFetchRequested = true;
	schedule.waitStartedAt = 0;
	schedule.waitDuration = 0;
	schedule.failuresInRow = 0;
	schedule.hasSucceeded = false;
	schedule.lastSuccessAt = 0;
}

// Check whether a download should start now (the elapsed-time check stays correct across millis() rollover)
bool isFetchDue(const FetchSchedule &schedule, uint32_t nowMs) {
	if (schedule.isFetchInProgress) {
		return false;
	}
	return schedule.isFetchRequested || nowMs - schedule.waitStartedAt >= schedule.waitDuration;
}

// Mark a download as started (it covers any download asked for until now)
void startFetch(FetchSchedule &schedule) {
	schedule.isFetchInProgress = true;
	schedule.isFetchRequested = false;
}

// Schedule the next download: the normal interval after a success, a longer and longer wait after failures in a row
void completeFetch(FetchSchedule &schedule, FetchOutcome outcome, uint32_t completedAt) {
	schedule.isFetchInProgress = false;
	schedule.waitStartedAt = completedAt;

	// A download that worked: the data is fresh, the next one comes at the normal interval
	if (outcome == FetchOutcome::Success) {
		schedule.waitDuration = FETCH_INTERVAL_MS;
		schedule.failuresInRow = 0;
		schedule.hasSucceeded = true;
		schedule.lastSuccessAt = completedAt;
		return;
	}

	// A failure: wait twice as long as after the previous one, up to the longest wait
	if (schedule.failuresInRow < 255) {
		schedule.failuresInRow++;
	}
	uint32_t retryDelay = FETCH_RETRY_MIN_MS;
	for (uint8_t failure = 1; failure < schedule.failuresInRow && retryDelay < FETCH_RETRY_MAX_MS; failure++) {
		retryDelay *= 2;
	}
	if (retryDelay > FETCH_RETRY_MAX_MS) {
		retryDelay = FETCH_RETRY_MAX_MS;
	}

	// adsb.fi asked to slow down: wait at least the rate limit pause
	if (outcome == FetchOutcome::RateLimited && retryDelay < FETCH_RATE_LIMITED_MS) {
		retryDelay = FETCH_RATE_LIMITED_MS;
	}
	schedule.waitDuration = retryDelay;
}

// Ask for a download right away (if one is running, the next one starts as soon as it ends)
void requestFetch(FetchSchedule &schedule) {
	schedule.isFetchRequested = true;
}

// Check whether the data on screen is old enough to warn about (only once a download has worked: before that, the display shows the error itself)
bool isDataStale(const FetchSchedule &schedule, uint32_t nowMs) {
	return schedule.hasSucceeded && nowMs - schedule.lastSuccessAt >= DATA_STALE_AFTER_MS;
}

// Get how old the data on screen is, in whole seconds
uint32_t getDataAgeSeconds(const FetchSchedule &schedule, uint32_t nowMs) {
	return schedule.hasSucceeded ? (nowMs - schedule.lastSuccessAt) / 1000UL : 0;
}
