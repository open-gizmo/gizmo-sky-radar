#ifndef RADAR_VIEW_H
#define RADAR_VIEW_H

#include "panel.h"
#include "theme.h"
#include "../config/radar_settings.h"
#include "../flights/flights.h"

// Drawing of the radar screen into a full-frame sprite (the display pushes the finished frame in one go, so nothing flickers)

// Geometry of the screen, in pixels
constexpr int RADAR_CENTER = DISPLAY_SIZE / 2;
constexpr int RADAR_RING_RADIUS = 104; // The outer ring: the range of the radar
constexpr int RADAR_EDGE_RADIUS = 119; // The edge of the round glass

// How long the beam takes to go round, in milliseconds
constexpr uint32_t SWEEP_PERIOD_MS = 4000;

// Paint the background that never changes between frames: a glow toward the center and the bezel (rebuilt when the theme changes)
void drawRadarBackground(LGFX_Sprite &canvas, const RadarPalette &palette);

// Draw the beam, the rings and the compass over the background (the empty radar of the status screens, too)
void drawRadarScope(LGFX_Sprite &canvas, const RadarPalette &palette, const RadarSettings &settings, uint32_t nowMs);

// Draw the aircraft, their trails and labels, the spotlight on the nearest ones and the line of status at the top
void drawRadarTraffic(LGFX_Sprite &canvas, const RadarPalette &palette, const RadarSettings &settings, const AircraftTracker &tracker, const FlightDataState &flightState, uint32_t nowMs);

#endif
