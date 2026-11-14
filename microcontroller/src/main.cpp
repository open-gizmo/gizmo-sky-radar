#include "serial/serial.h"
#include "storage/storage.h"
#include "display/display.h"
#include "wifi/wifi.h"
#include "flights/flights.h"
#include "server/server.h"
#include <gizmo/device.h>

// Setup
void setup() {
	initSerial(); // Init serial
	setupStorage(); // Load the saved settings
	setupDisplay(); // Start the display and play the boot animation
	setupWiFi(); // Start the Wi-Fi setup and its web API
	setupServer(); // Setup server

	// Factory reset: hold the BOOT button for 10 s to forget the Wi-Fi and the settings (the display counts down)
	gizmo::FactoryResetOptions resetOptions;
	resetOptions.clearSettings = [] {
		showFactoryResetDone();
		clearSavedRadarSettings();
	};
	resetOptions.onHold = showFactoryResetCountdown;
	resetOptions.onCancel = hideFactoryResetCountdown;
	gizmo::setupFactoryReset(resetOptions);

	manageWiFiConnection(); // Manage the WiFi connection
	setupFlights(); // Start downloading the flight data, in a task of its own
}

// Loop
void loop() {
	// Restart the device if the web server asked for it, and watch the BOOT button
	gizmo::manageDevice();

	// Manage the WiFi connection
	manageWiFiConnection();

	// Draw the next frame of the display
	updateDisplay();

	// Give the CPU back between frames
	delay(1);
}
