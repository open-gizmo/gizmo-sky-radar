#ifndef CONFIG_H
#define CONFIG_H

#include <ESPAsyncWebServer.h>
#include "radar_settings.h"

// Project name, shown by the device API, the web app and the display, and its machine name (the mDNS hostname and the MCP server name)
constexpr const char *PROJECT_NAME = "Gizmo Sky Radar";
constexpr const char *PROJECT_ID = "gizmo-sky-radar";

// Name of the open setup hotspot (shown on the display too)
constexpr const char *ACCESS_POINT_NAME = "Gizmo-Sky-Radar";

// Firmware version shown in the web app and to MCP clients (the release build sets it)
#ifndef PIO_FIRMWARE_VERSION
#define PIO_FIRMWARE_VERSION "dev"
#endif

// Web server shared by the Wi-Fi setup, the device API and the radar API
extern AsyncWebServer server;

// Get a copy of the current settings (safe from any task)
RadarSettings getRadarSettings();

// Replace the current settings (safe from any task; saving them is the job of the storage)
void setRadarSettings(const RadarSettings &settings);

// A number that changes every time the settings change, so the display and the flight data know when to catch up
uint32_t getSettingsRevision();

#endif
