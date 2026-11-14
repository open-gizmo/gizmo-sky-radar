#include "flights.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <gizmo/text.h>
#include <gizmo/wifi_setup.h>
#include "aircraft_parser.h"
#include "fetch_schedule.h"
#include "../config/config.h"
#include "../serial/serial.h"

/*
A task of its own downloads the aircraft around the radar from adsb.fi every few seconds,
reads the nearest ones and hands them to the tracker, which follows them across downloads.
The display and the web server copy the tracker under a lock, so they never wait for the network.
*/

namespace {
	constexpr uint32_t TASK_STACK_SIZE = 12288; // TLS needs room
	constexpr UBaseType_t TASK_PRIORITY = 1;
	constexpr BaseType_t TASK_CORE = 0; // The display runs in the loop on core 1
	constexpr uint32_t IDLE_DELAY_MS = 200; // Between checks while there's nothing to download
	constexpr size_t MAX_RESPONSE_SIZE = 768 * 1024; // A very busy sky at the largest range is a few hundred KB
	constexpr uint32_t HTTP_TIMEOUT_MS = 10000;
	constexpr const char *USER_AGENT = "GizmoSkyRadar/" PIO_FIRMWARE_VERSION " (+https://opengizmo.dev/projects/gizmo-sky-radar)";

	// Allocate the JSON of a download in the PSRAM, where a busy sky fits (the internal RAM when there is none)
	class SpiRamAllocator : public ArduinoJson::Allocator {
	public:
		void *allocate(size_t size) override {
			void *pointer = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
			return pointer != nullptr ? pointer : malloc(size);
		}
		void deallocate(void *pointer) override {
			heap_caps_free(pointer);
		}
		void *reallocate(void *pointer, size_t newSize) override {
			void *result = heap_caps_realloc(pointer, newSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
			return result != nullptr ? result : realloc(pointer, newSize);
		}
	};
	SpiRamAllocator spiRamAllocator;

	// What the task shares with the readers, under the lock
	SemaphoreHandle_t flightsLock = nullptr;
	AircraftTracker sharedTracker;
	FetchSchedule schedule;
	FlightDataState sharedState = { FlightDataStatus::NoLocation, false, 0, 0, 0, 0, "" };
	bool isTrackerResetRequested = false;

	// Working memory of the task (static: too big for its stack)
	Aircraft parsedAircraft[MAX_AIRCRAFT];
	uint8_t *responseBuffer = nullptr;

	// Run a piece of code under the lock
	template <typename Function>
	void withLock(Function function) {
		xSemaphoreTake(flightsLock, portMAX_DELAY);
		function();
		xSemaphoreGive(flightsLock);
	}

	// The status of the data, from what the task knows
	FlightDataStatus computeStatus(const RadarSettings &settings, uint32_t nowMs) {
		if (!settings.hasLocation) {
			return FlightDataStatus::NoLocation;
		}
		if (!schedule.hasSucceeded) {
			if (!gizmo::isWifiConnected()) {
				return FlightDataStatus::WaitingForNetwork;
			}
			return sharedState.lastError[0] != '\0' ? FlightDataStatus::Error : FlightDataStatus::Loading;
		}
		return isDataStale(schedule, nowMs) ? FlightDataStatus::Stale : FlightDataStatus::Live;
	}

	// Read the body of the answer into the buffer (false when it's too big or doesn't arrive whole)
	bool readResponseBody(HTTPClient &http, size_t &length) {
		auto *stream = http.getStreamPtr();
		const int expectedSize = http.getSize(); // -1 when the server doesn't say, then the body ends when it closes
		length = 0;
		uint32_t lastDataAt = millis();
		while (expectedSize < 0 || length < static_cast<size_t>(expectedSize)) {
			const int available = stream->available();
			if (available > 0) {
				if (length + available > MAX_RESPONSE_SIZE) {
					return false;
				}
				length += stream->readBytes(responseBuffer + length, available);
				lastDataAt = millis();
			} else if (!stream->connected()) {
				break;
			} else if (millis() - lastDataAt > HTTP_TIMEOUT_MS) {
				return false;
			} else {
				delay(2);
			}
		}
		return expectedSize < 0 || length == static_cast<size_t>(expectedSize);
	}

	// Download and read the aircraft around the radar, writing the error message when it fails
	FetchOutcome downloadFlights(const RadarSettings &settings, size_t &count, uint16_t &areaTotal, int &httpStatus, char *error, size_t errorSize) {
		count = 0;
		areaTotal = 0;
		httpStatus = 0;

		// The URL of the area around the radar
		const GeoPoint center = { settings.latitude, settings.longitude };
		const uint16_t radiusNm = getQueryRadiusNm(settings.rangeKm);
		char url[128];
		buildFlightDataUrl(url, sizeof(url), center, radiusNm);
		printLogfln("Downloading %s", url);

		// Ask adsb.fi (HTTP/1.0, so the body never comes in chunks; the certificate isn't checked, the data is public)
		WiFiClientSecure client;
		client.setInsecure();
		HTTPClient http;
		http.useHTTP10(true);
		http.setTimeout(HTTP_TIMEOUT_MS);
		http.setConnectTimeout(HTTP_TIMEOUT_MS);
		http.setUserAgent(USER_AGENT);
		if (!http.begin(client, url)) {
			snprintf(error, errorSize, "Can't reach adsb.fi");
			return FetchOutcome::Failure;
		}
		http.addHeader("Accept", "application/json");
		httpStatus = http.GET();

		// No answer, or an error status
		if (httpStatus <= 0) {
			snprintf(error, errorSize, "No answer from adsb.fi");
			http.end();
			return FetchOutcome::Failure;
		}
		if (httpStatus == 429) {
			snprintf(error, errorSize, "adsb.fi asks to slow down");
			http.end();
			return FetchOutcome::RateLimited;
		}
		if (httpStatus != 200) {
			snprintf(error, errorSize, "adsb.fi answered %d", httpStatus);
			http.end();
			return FetchOutcome::Failure;
		}

		// Read the whole body, then the fields the radar needs
		size_t length = 0;
		const bool isBodyRead = readResponseBody(http, length);
		http.end();
		if (!isBodyRead) {
			snprintf(error, errorSize, "The download was cut off");
			return FetchOutcome::Failure;
		}
		JsonDocument filter;
		buildFlightDataFilter(filter);
		JsonDocument doc(&spiRamAllocator);
		const DeserializationError parseError = deserializeJson(doc, responseBuffer, length, DeserializationOption::Filter(filter));
		if (parseError) {
			printLogfln("Unreadable flight data: %s", parseError.c_str());
			snprintf(error, errorSize, "Unreadable flight data");
			return FetchOutcome::Failure;
		}

		// Keep the nearest aircraft of the area
		const ParseResult result = parseFlightData(doc.as<JsonVariantConst>(), center, radiusNm * NAUTICAL_MILE_KM, settings.showGroundTraffic, parsedAircraft, MAX_AIRCRAFT);
		if (!result.isValid) {
			snprintf(error, errorSize, "Unexpected answer from adsb.fi");
			return FetchOutcome::Failure;
		}
		count = result.count;
		areaTotal = result.totalCount > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(result.totalCount);
		printLogfln("%u aircraft in the area, %u kept (%u bytes)", static_cast<unsigned>(result.totalCount), static_cast<unsigned>(result.count), static_cast<unsigned>(length));
		return FetchOutcome::Success;
	}

	// The download task: download when due, while the radar has a location and a network
	void flightsTask(void *parameter) {
		for (;;) {
			const RadarSettings settings = getRadarSettings();
			const uint32_t nowMs = millis();

			// Decide whether to download now, and keep the status up to date
			bool isDue = false;
			withLock([&] {
				if (isTrackerResetRequested) {
					resetTracker(sharedTracker);
					resetFetchSchedule(schedule);
					sharedState.lastError[0] = '\0';
					sharedState.areaTotal = 0;
					isTrackerResetRequested = false;
				}
				isDue = settings.hasLocation && gizmo::isWifiConnected() && isFetchDue(schedule, nowMs);
				if (isDue) {
					startFetch(schedule);
				}
				sharedState.status = computeStatus(settings, nowMs);
				sharedState.hasData = schedule.hasSucceeded;
				sharedState.dataAgeSeconds = getDataAgeSeconds(schedule, nowMs);
			});
			if (!isDue) {
				vTaskDelay(pdMS_TO_TICKS(IDLE_DELAY_MS));
				continue;
			}

			// Download (without the lock: it takes a second or two)
			size_t count = 0;
			uint16_t areaTotal = 0;
			int httpStatus = 0;
			char error[FLIGHT_ERROR_SIZE] = "";
			const FetchOutcome outcome = downloadFlights(settings, count, areaTotal, httpStatus, error, sizeof(error));
			const uint32_t completedAt = millis();

			// Hand the aircraft to the tracker, unless the location changed meanwhile (their distances are from the old one)
			withLock([&] {
				const RadarSettings latestSettings = getRadarSettings();
				const bool isSameLocation = latestSettings.latitude == settings.latitude && latestSettings.longitude == settings.longitude && !isTrackerResetRequested;
				if (outcome == FetchOutcome::Success && isSameLocation) {
					updateTracker(sharedTracker, parsedAircraft, count, completedAt);
					sharedState.areaTotal = areaTotal;
					sharedState.updateCount++;
					sharedState.lastError[0] = '\0';
				} else if (outcome != FetchOutcome::Success) {
					gizmo::copyString(sharedState.lastError, error, sizeof(sharedState.lastError));
					printLogfln("Download failed: %s", error);
				}
				sharedState.lastHttpStatus = httpStatus;

				// Schedule the next download (right away for a new location, whatever happened to this one)
				if (isSameLocation) {
					completeFetch(schedule, outcome, completedAt);
				} else {
					schedule.isFetchInProgress = false;
					requestFetch(schedule);
				}
			});
		}
	}
}

// Start the download task
void setupFlights() {
	flightsLock = xSemaphoreCreateMutex();
	resetTracker(sharedTracker);
	resetFetchSchedule(schedule);

	// The buffer of the downloads, in the PSRAM
	responseBuffer = static_cast<uint8_t *>(heap_caps_malloc(MAX_RESPONSE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	if (responseBuffer == nullptr) {
		printLogln("No PSRAM for the flight data: the radar can't download");
		gizmo::copyString(sharedState.lastError, "Not enough memory", sizeof(sharedState.lastError));
		return;
	}
	xTaskCreatePinnedToCore(flightsTask, "flights", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, nullptr, TASK_CORE);
}

// Download again right away
void requestFlightRefresh(bool isLocationChanged) {
	withLock([&] {
		if (isLocationChanged) {
			isTrackerResetRequested = true;
		}
		requestFetch(schedule);
	});
}

// Copy the aircraft followed and the state of the data
void readFlights(AircraftTracker &tracker, FlightDataState &state) {
	withLock([&] {
		tracker.count = sharedTracker.count;
		tracker.lastUpdateMs = sharedTracker.lastUpdateMs;
		memcpy(tracker.items, sharedTracker.items, sizeof(TrackedAircraft) * sharedTracker.count);
		state = sharedState;
	});
}

// Copy the state of the data only
void readFlightState(FlightDataState &state) {
	withLock([&] {
		state = sharedState;
	});
}

// Name of a status in the web API
const char *getFlightStatusName(FlightDataStatus status) {
	switch (status) {
		case FlightDataStatus::NoLocation:
			return "no_location";
		case FlightDataStatus::WaitingForNetwork:
			return "waiting_for_network";
		case FlightDataStatus::Loading:
			return "loading";
		case FlightDataStatus::Live:
			return "live";
		case FlightDataStatus::Stale:
			return "stale";
		default:
			return "error";
	}
}
