#ifndef FETCH_SCHEDULE_H
#define FETCH_SCHEDULE_H

#include <stdint.h>

// When the next download of the flight data is due, kept free of the hardware so the native tests can drive it (times are millis(), 32-bit like on the ESP32)

constexpr uint32_t FETCH_INTERVAL_MS = 5000; // Time between two downloads: the aircraft move smoothly in between (adsb.fi asks for no more than one request per second)
constexpr uint32_t FETCH_RETRY_MIN_MS = 5000; // Wait after the first failed download, doubled at each failure in a row
constexpr uint32_t FETCH_RETRY_MAX_MS = 60000; // Longest wait between failed downloads
constexpr uint32_t FETCH_RATE_LIMITED_MS = 30000; // Wait after adsb.fi answers "too many requests"
constexpr uint32_t DATA_STALE_AFTER_MS = 60000; // Data this old means the downloads keep failing, so the display says so

// Outcome of a download
enum class FetchOutcome : uint8_t {
	Success,
	Failure, // No answer, an error status, or data that couldn't be read
	RateLimited // adsb.fi asked to slow down (HTTP 429)
};

struct FetchSchedule {
	bool isFetchInProgress;
	bool isFetchRequested; // A download asked for right away (for example after a change of location)
	uint32_t waitStartedAt; // When the wait for the next download started
	uint32_t waitDuration; // How long to wait before the next download
	uint8_t failuresInRow; // Failed downloads since the last one that worked
	bool hasSucceeded; // A download has worked since boot (or since the location changed)
	uint32_t lastSuccessAt; // When the data on screen was downloaded
};

void resetFetchSchedule(FetchSchedule &schedule);
bool isFetchDue(const FetchSchedule &schedule, uint32_t nowMs);
void startFetch(FetchSchedule &schedule);
void completeFetch(FetchSchedule &schedule, FetchOutcome outcome, uint32_t completedAt);
void requestFetch(FetchSchedule &schedule);
bool isDataStale(const FetchSchedule &schedule, uint32_t nowMs);
uint32_t getDataAgeSeconds(const FetchSchedule &schedule, uint32_t nowMs);

#endif
