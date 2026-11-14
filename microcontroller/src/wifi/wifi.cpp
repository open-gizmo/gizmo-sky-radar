#include <Arduino.h>
#include "wifi.h"
#include <WiFi.h>
#include <gizmo/wifi_setup.h>
#include "../config/config.h"

/*
The Wi-Fi connection, the setup hotspot and its web API come from gizmo-common.
This module only gives them the radar's names, and tells the display how the connection is doing.
*/

namespace {
	// Wi-Fi setup options of the radar
	const gizmo::WifiSetupOptions WIFI_SETUP_OPTIONS = {
		ACCESS_POINT_NAME, // Setup hotspot name
		PROJECT_ID, // mDNS hostname, always the project name (http://gizmo-sky-radar.local)
		"/ap-setup", // Setup page of the web app
		"/_nuxt/" // Files of the web app, never redirected
	};
}

// Start the Wi-Fi setup and register its web API (call before the web server starts)
void setupWiFi() {
	gizmo::setupWifi(server, WIFI_SETUP_OPTIONS);
	WiFi.setSleep(false); // Keep the radio awake: with power saving on, the downloads and the web app answer much later
}

// Manage the Wi-Fi connection and the setup hotspot
void manageWiFiConnection() {
	gizmo::manageWifi();
}

// Get the state of the connection
NetworkState getNetworkState() {
	if (gizmo::isWifiConnected()) {
		return NetworkState::Connected;
	}
	if (gizmo::isAccessPointActive() && gizmo::getWifiStatus() != gizmo::WifiStatus::Connecting) {
		return NetworkState::SetupHotspot;
	}
	return NetworkState::Connecting;
}
