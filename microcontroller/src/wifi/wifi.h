#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>

// State of the connection, for the display
enum class NetworkState : uint8_t {
	Connected,
	Connecting,
	SetupHotspot // No working network: the setup hotspot is open
};

void setupWiFi();
void manageWiFiConnection();

// Get the state of the connection
NetworkState getNetworkState();

#endif
