# Copilot Instructions

## What This Project Is

Gizmo Sky Radar is a two-part project that turns a Seeed Studio XIAO ESP32-S3 and a GC9A01 round display into a live plane radar.

- `microcontroller/` contains the PlatformIO firmware for the XIAO ESP32-S3 (8 MB PSRAM) and the 240 × 240 GC9A01 display. It downloads the aircraft around a location from adsb.fi, draws the radar, and serves the web app, the radar API and a Markdown guide for AI agents (`/llms.txt`).
- `webapp/` contains the Nuxt 4 web app, embedded in the firmware, used to set up the Wi-Fi, set the location and the display, and watch the radar live.

The firmware is the runtime of the physical radar. The webapp mirrors its API, and the mock device API in `webapp/server/` answers like the firmware during development, with a simulated sky.

## Core Concepts

- Firmware code is organized by responsibility under `microcontroller/src/` (`config`, `display`, `flights`, `guide`, `serial`, `server`, `storage`, `wifi`).
- The flight data is downloaded by a FreeRTOS task of its own (`flights/flights.cpp`) on core 0; the display draws in the main loop on core 1 into a full-screen sprite in PSRAM, and both copy the tracker under a mutex.
- Aircraft move on between downloads by dead reckoning (`flights/aircraft_tracker.*`), on the display and in the web app alike.
- The Wi-Fi setup, the captive portal, the device API, the factory reset, logging and the embedded assets come from the shared [gizmo-common](https://github.com/open-gizmo/gizmo-common) library, pinned to an exact release.
- The web interface is a Nuxt app under `webapp/app/` that extends the gizmo-common web layer (dashboard layout, Wi-Fi pages, device card).
- Changes to the radar API (`/api/config`, `/api/settings`, `/api/reset-settings`, `/api/aircraft`) need coordinated updates in the firmware, the webapp, and the mock device API.

## Working Rules

1. Keep the split between firmware and webapp clear. Do not mix embedded-device logic into the frontend or frontend-specific behavior into firmware modules.
2. Preserve existing module boundaries in `microcontroller/src/`; prefer extending the relevant subsystem instead of adding cross-cutting logic to `main.cpp`.
3. Keep hardware-free logic (`config/radar_settings.*`, `display/units.*`, everything in `flights/` but `flights.cpp`, and everything in `guide/`) free of hardware headers, and test it with `pio test -e native`.
4. ArduinoJson 7 keeps a pointer to a `const char *` instead of copying it: copy any text that isn't a literal (`JsonString(text, JsonString::Copied)`).
5. When changing settings, routes, payloads, or validation rules, update the firmware handlers, the mock device API and the webapp together, and translate every new error ID.
6. The colors of the altitudes and of the themes live in `display/theme.cpp` and `webapp/app/composables/useUtils.js`: change both together.
7. Never call a blocking network request from the main loop: the display must keep its frame rate. Never draw with `fillArc` or `drawWideLine` on the full-screen sprite: they test every pixel of their bounding box and cost a frame.
8. Prefer minimal, hardware-aware changes in firmware: avoid unnecessary dynamic allocation, blocking behavior, and broad refactors unless they are required.
