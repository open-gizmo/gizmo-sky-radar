#include "display.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <gizmo/device.h>
#include <gizmo/wifi_setup.h>
#include "panel.h"
#include "radar_view.h"
#include "theme.h"
#include "../config/config.h"
#include "../flights/flights.h"
#include "../serial/serial.h"
#include "../wifi/wifi.h"

/*
Every frame is drawn into a sprite the size of the screen, then pushed in one go, so nothing flickers.
The background that never changes (the glow and the bezel) waits in a second sprite, copied in at the start of each frame.
*/

namespace {
	constexpr uint32_t FRAME_INTERVAL_MS = 33; // About 30 frames a second
	constexpr uint32_t BOOT_ANIMATION_MS = 2200;
	constexpr size_t FRAME_BYTES = DISPLAY_SIZE * DISPLAY_SIZE * 2; // 16-bit color

	RadarPanel panel;
	LGFX_Sprite frame(&panel);
	LGFX_Sprite background(&panel);
	bool isFrameReady = false;

	// The aircraft as the display last copied them (in the PSRAM: too big for the internal RAM)
	AircraftTracker *tracker = nullptr;

	// What the display last applied from the settings
	uint32_t appliedRevision = UINT32_MAX;
	RadarSettings settings = getDefaultRadarSettings();

	// Pacing, and the countdown of the factory reset
	uint32_t lastFrameAt = 0;
	bool isResetCountdownShown = false;
	unsigned long resetRemainingMs = 0;

	// Frame timing, logged in debug builds
	uint32_t frameCount = 0;
	uint32_t frameTimeTotalUs = 0;
	uint32_t pushTimeTotalUs = 0;
	uint32_t lastTimingLogAt = 0;

	// The palette of the current theme
	const RadarPalette &palette() {
		return getRadarPalette(settings.theme);
	}

	// Apply the settings that changed: brightness, rotation, and the background of a new theme
	void applySettings() {
		const uint32_t revision = getSettingsRevision();
		if (revision == appliedRevision) {
			return;
		}
		const RadarTheme previousTheme = settings.theme;
		const bool isFirstApply = appliedRevision == UINT32_MAX;
		settings = getRadarSettings();
		appliedRevision = revision;

		// Brightness (the eye sees it on a curve, so the low end gets finer steps) and rotation
		const uint32_t level = static_cast<uint32_t>(settings.brightness) * settings.brightness * 255 / (BRIGHTNESS_MAX * BRIGHTNESS_MAX);
		panel.setBrightness(level < 4 ? 4 : level);
		panel.setRotation(settings.rotation);

		// The background of the theme
		if (isFirstApply || previousTheme != settings.theme) {
			drawRadarBackground(background, palette());
		}
	}

	// Start a frame from the background
	void startFrame() {
		memcpy(frame.getBuffer(), background.getBuffer(), FRAME_BYTES);
	}

	// Push the finished frame to the display
	void pushFrame() {
		frame.pushSprite(0, 0);
	}

	// Draw a box of text in the middle of the radar: a title, then lines (the highlighted one bigger and brighter)
	void drawMessage(const char *title, const char *const *lines, size_t lineCount, int highlightedLine) {
		const RadarPalette &colors = palette();

		// A dark round panel, so the text reads over the beam
		frame.fillCircle(RADAR_CENTER, RADAR_CENTER, 78, colors.backgroundEdge);
		frame.drawCircle(RADAR_CENTER, RADAR_CENTER, 78, colors.gridBright);

		// The title
		frame.setTextDatum(textdatum_t::middle_center);
		frame.setFont(&fonts::Font0);
		frame.setTextColor(colors.accent);
		int y = RADAR_CENTER - static_cast<int>(lineCount) * 8 - 6;
		frame.drawString(title, RADAR_CENTER, y);
		y += 18;

		// The lines
		for (size_t index = 0; index < lineCount; index++) {
			const bool isHighlighted = static_cast<int>(index) == highlightedLine;
			if (isHighlighted) {
				frame.setFont(&fonts::FreeSansBold9pt7b);
			} else {
				frame.setFont(&fonts::Font2);
			}
			frame.setTextColor(isHighlighted ? colors.sweep : colors.text);
			frame.drawString(lines[index], RADAR_CENTER, y);
			y += isHighlighted ? 20 : 16;
		}
	}

	// The setup screen: join the hotspot from a phone
	void drawSetupScreen(uint32_t nowMs) {
		startFrame();
		drawRadarScope(frame, palette(), settings, nowMs);
		const char *lines[] = { "Join the Wi-Fi", ACCESS_POINT_NAME, "from your phone", "to set me up" };
		drawMessage("WI-FI SETUP", lines, 4, 1);
	}

	// The screen while the radar connects to its network
	void drawConnectingScreen(uint32_t nowMs) {
		char ssid[gizmo::WIFI_SSID_SIZE] = "";
		gizmo::getSavedWifiSsid(ssid, sizeof(ssid));
		char dots[4] = "...";
		dots[(nowMs / 400) % 4] = '\0';
		char title[24];
		snprintf(title, sizeof(title), "CONNECTING%s", dots);
		startFrame();
		drawRadarScope(frame, palette(), settings, nowMs);
		const char *lines[] = { "Joining", ssid[0] != '\0' ? ssid : "Wi-Fi" };
		drawMessage(title, lines, 2, 1);
	}

	// The screen of a radar without a location: where to set it
	void drawNoLocationScreen(uint32_t nowMs) {
		char address[20];
		snprintf(address, sizeof(address), "%s", WiFi.localIP().toString().c_str());
		startFrame();
		drawRadarScope(frame, palette(), settings, nowMs);
		const char *lines[] = { "Set my location at", "gizmo-sky-radar", ".local", address };
		drawMessage("ALMOST READY", lines, 4, 1);
	}

	// The radar, with its traffic
	void drawRadarScreen(uint32_t nowMs) {
		FlightDataState flightState;
		readFlights(*tracker, flightState);
		startFrame();
		drawRadarScope(frame, palette(), settings, nowMs);
		drawRadarTraffic(frame, palette(), settings, *tracker, flightState, nowMs);
	}

	// The countdown of the factory reset: a ring that fills, and the seconds left
	void drawResetScreen() {
		const RadarPalette &colors = palette();
		frame.fillScreen(static_cast<uint32_t>(0x000000));
		const float progress = 1.0f - static_cast<float>(resetRemainingMs) / gizmo::FACTORY_RESET_HOLD_MS;
		frame.fillArc(RADAR_CENTER, RADAR_CENTER, RADAR_EDGE_RADIUS - 2, RADAR_EDGE_RADIUS - 10, -90.0f, -90.0f + 360.0f * progress, colors.warning);
		frame.setTextDatum(textdatum_t::middle_center);
		frame.setFont(&fonts::Font0);
		frame.setTextColor(colors.warning);
		frame.drawString("FACTORY RESET", RADAR_CENTER, 62);
		frame.setFont(&fonts::FreeSansBold24pt7b);
		frame.setTextColor(colors.accent);
		char seconds[8];
		snprintf(seconds, sizeof(seconds), "%lu", (resetRemainingMs + 999) / 1000);
		frame.drawString(seconds, RADAR_CENTER, RADAR_CENTER);
		frame.setFont(&fonts::Font2);
		frame.setTextColor(colors.text);
		frame.drawString("Release the button", RADAR_CENTER, 166);
		frame.drawString("to cancel", RADAR_CENTER, 182);
	}

	// The boot animation: the beam goes round once over the name
	void playBootAnimation() {
		const uint32_t startedAt = millis();
		RadarSettings bootSettings = settings;
		bootSettings.showSweep = true;
		char version[24];
		snprintf(version, sizeof(version), "%s", PIO_FIRMWARE_VERSION);
		while (millis() - startedAt < BOOT_ANIMATION_MS) {
			const uint32_t elapsed = millis() - startedAt;
			startFrame();
			drawRadarScope(frame, palette(), bootSettings, elapsed + SWEEP_PERIOD_MS * 3 / 4);
			frame.fillRoundRect(RADAR_CENTER - 78, 92, 156, 58, 10, palette().backgroundEdge);
			frame.setTextDatum(textdatum_t::middle_center);
			frame.setFont(&fonts::Font0);
			frame.setTextColor(palette().textDim);
			frame.drawString("G I Z M O", RADAR_CENTER, 102);
			frame.setFont(&fonts::Orbitron_Light_24);
			frame.setTextColor(palette().sweep);
			frame.drawString("SKY RADAR", RADAR_CENTER, 122);
			frame.setFont(&fonts::Font0);
			frame.setTextColor(palette().textDim);
			frame.drawString(version, RADAR_CENTER, 141);
			pushFrame();
			delay(5);
		}
	}

	// Log how long the frames take to draw and to push, in debug builds
	void logFrameTiming(uint32_t drawTimeUs, uint32_t pushTimeUs) {
		frameCount++;
		frameTimeTotalUs += drawTimeUs;
		pushTimeTotalUs += pushTimeUs;
		if (millis() - lastTimingLogAt >= 10000) {
			const uint32_t frames = frameCount > 0 ? frameCount : 1;
			printLogfln("Display: %lu frames, %lu us to draw and %lu us to push each, free heap %lu, free PSRAM %lu", static_cast<unsigned long>(frameCount), static_cast<unsigned long>(frameTimeTotalUs / frames), static_cast<unsigned long>(pushTimeTotalUs / frames), static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getFreePsram()));
			frameCount = 0;
			frameTimeTotalUs = 0;
			pushTimeTotalUs = 0;
			lastTimingLogAt = millis();
		}
	}
}

// Start the display and play the boot animation
void setupDisplay() {
	panel.init();
	panel.setBrightness(0);
	panel.fillScreen(static_cast<uint32_t>(0x000000));

	// The frame and the background, in the PSRAM (the internal RAM is for Wi-Fi and TLS)
	frame.setColorDepth(16);
	frame.setPsram(true);
	background.setColorDepth(16);
	background.setPsram(true);
	tracker = static_cast<AircraftTracker *>(heap_caps_calloc(1, sizeof(AircraftTracker), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	isFrameReady = frame.createSprite(DISPLAY_SIZE, DISPLAY_SIZE) != nullptr && background.createSprite(DISPLAY_SIZE, DISPLAY_SIZE) != nullptr && tracker != nullptr;
	if (!isFrameReady) {
		printLogln("Not enough memory for the display");
		panel.setBrightness(128);
		panel.setTextDatum(textdatum_t::middle_center);
		panel.drawString("Not enough memory", RADAR_CENTER, RADAR_CENTER);
		return;
	}

	// The settings, then the boot animation
	applySettings();
	playBootAnimation();
}

// Draw the next frame when it's due
void updateDisplay() {
	if (!isFrameReady) {
		return;
	}

	// One frame every interval
	const uint32_t nowMs = millis();
	if (nowMs - lastFrameAt < FRAME_INTERVAL_MS) {
		return;
	}
	lastFrameAt = nowMs;
	const uint32_t startedAt = micros();
	applySettings();

	// The screen for the state of the device
	if (isResetCountdownShown) {
		drawResetScreen();
	} else {
		switch (getNetworkState()) {
			case NetworkState::SetupHotspot:
				drawSetupScreen(nowMs);
				break;
			case NetworkState::Connecting:
				drawConnectingScreen(nowMs);
				break;
			default:
				if (settings.hasLocation) {
					drawRadarScreen(nowMs);
				} else {
					drawNoLocationScreen(nowMs);
				}
				break;
		}
	}
	const uint32_t drawnAt = micros();
	pushFrame();
	logFrameTiming(drawnAt - startedAt, micros() - drawnAt);
}

// Show the countdown of the factory reset
void showFactoryResetCountdown(unsigned long remainingMs) {
	isResetCountdownShown = true;
	resetRemainingMs = remainingMs;
}

// Hide the countdown of the factory reset
void hideFactoryResetCountdown() {
	isResetCountdownShown = false;
}

// Show that the factory reset is happening
void showFactoryResetDone() {
	if (!isFrameReady) {
		return;
	}
	frame.fillScreen(static_cast<uint32_t>(0x000000));
	frame.setTextDatum(textdatum_t::middle_center);
	frame.setFont(&fonts::FreeSansBold9pt7b);
	frame.setTextColor(palette().warning);
	frame.drawString("Resetting...", RADAR_CENTER, RADAR_CENTER);
	pushFrame();
}
