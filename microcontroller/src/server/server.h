#ifndef SERVER_H
#define SERVER_H

#include <ArduinoJson.h>
#include <gizmo/http.h>
#include "../config/config.h"

// Standard API responses and CORS checks come from gizmo-common
using gizmo::ensureCorsAllowed;
using gizmo::sendCorsPreflightResponse;
using gizmo::sendErrorResponse;
using gizmo::sendSuccessResponse;

// Web server routes (the Wi-Fi setup and device routes are registered by gizmo-common)
void setupConfigGetRoute();
void setupSettingsPostRoute();
void setupResetSettingsPostRoute();
void setupAircraftGetRoute();
void setupMcpRoute();
void setupAgentGuideRoute();
void setupCaptivePortalRoute();

// Send the guide for AI agents (Markdown)
void sendAgentGuide(AsyncWebServerRequest *request);

// Write the aircraft around the radar (the center, the state of the data, then the aircraft nearest first, up to limit), for the web API and the MCP server
void writeNearbyAircraft(JsonObject result, size_t limit, bool includeTrails);

// Write the status of the radar: its location, its range, its settings and the state of its data
void writeRadarStatus(JsonObject result);

// Server setup
void setupRoutes();
void setupServer();

#endif
