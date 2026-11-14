# Project Overview

This repository contains ESP32-S3 firmware for a live plane radar on a GC9A01 round display, with an embedded web UI.

## Structure

- `src/main.cpp`: entry point; initializes serial logging, the saved settings, the display, Wi-Fi, the web server, the factory reset and the flight data task.
- `src/config`: the project name, the pins of the display (`pins.h`), the shared web server, and the settings of the radar with their limits and JSON (`radar_settings.*`, hardware-free).
- `src/storage`: saves the settings with `Preferences` (gizmo-common stores the Wi-Fi credentials).
- `src/flights`: downloads the aircraft from adsb.fi in a task of its own (`flights.*`), and the hardware-free parts: the readsb JSON parser (`aircraft_parser.*`), the tracker that keeps trails and moves the aircraft on between downloads (`aircraft_tracker.*`), the download schedule with its backoff (`fetch_schedule.*`), the geography (`geo.*`) and the JSON of an aircraft for the API (`aircraft_json.*`).
- `src/display`: the LovyanGFX driver of the GC9A01 (`panel.h`), the screens (`display.*`), the radar drawing (`radar_view.*`), the colors (`theme.*`) and the units (`units.*`, hardware-free).
- `src/wifi`: passes the radar's names to the gizmo-common Wi-Fi setup (connection, access point fallback, captive portal) and tells the display how the connection is doing.
- `src/mcp`: the MCP server for AI agents (`mcp_protocol.*`) and the Markdown guide for agents (`agent_guide.*`), both hardware-free.
- `src/server`: exposes the AsyncWebServer routes for the settings, the aircraft, the MCP server, the guide for agents and the embedded assets (the Wi-Fi setup and device routes come from gizmo-common).
- `src/serial`: serial debug logging helpers.
- `src/webui`: generated assets for the embedded front-end.

## Runtime Flow

The firmware boots, loads the saved settings, plays the boot animation, connects to Wi-Fi or opens the setup hotspot, and starts the download task. Every 5 seconds the task downloads the aircraft around the location and hands the nearest 64 to the tracker; at about 25 frames a second the main loop draws the radar from a copy of the tracker, moving each aircraft on along its track.
