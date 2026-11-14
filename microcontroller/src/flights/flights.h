#ifndef FLIGHTS_H
#define FLIGHTS_H

#include <stddef.h>
#include <stdint.h>
#include "aircraft_tracker.h"

// The flight data: downloaded from adsb.fi by a task of its own, so the display and the web server never wait for it

// Where the flight data stands
enum class FlightDataStatus : uint8_t {
	NoLocation, // The radar has no location yet: nothing to download
	WaitingForNetwork, // No Wi-Fi
	Loading, // The first download is on its way
	Live, // The last download worked
	Stale, // The downloads keep failing: the aircraft shown are getting old
	Error // No download has worked yet
};

// Size of the message of the last error, with its terminator
constexpr size_t FLIGHT_ERROR_SIZE = 48;

// The state of the flight data, for the display and the web API
struct FlightDataState {
	FlightDataStatus status;
	bool hasData; // A download has worked since boot (or since the location changed)
	uint32_t dataAgeSeconds; // Since the last download that worked
	uint32_t updateCount; // Downloads that worked since boot, so a reader can tell a new one arrived
	uint16_t areaTotal; // Aircraft in the downloaded area, before keeping the nearest ones
	int lastHttpStatus; // Of the last download (0 when it got no answer)
	char lastError[FLIGHT_ERROR_SIZE]; // Why the last download failed (empty after a success)
};

// Start the download task
void setupFlights();

// Download again right away (after a change of settings); a change of location also forgets the aircraft of the old one
void requestFlightRefresh(bool isLocationChanged);

// Copy the aircraft followed and the state of the data (safe from any task)
void readFlights(AircraftTracker &tracker, FlightDataState &state);

// Copy the state of the data only
void readFlightState(FlightDataState &state);

// Name of a status in the web API ("live", "no_location"...)
const char *getFlightStatusName(FlightDataStatus status);

#endif
