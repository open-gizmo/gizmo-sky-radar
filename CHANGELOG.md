# Changelog

What changed in each release. New notes go under **Unreleased** while the work is done; a release moves them under its version, and the release workflow publishes that section as the GitHub release notes (it refuses to release a version without them).

## Unreleased

The first release of Gizmo Sky Radar: a live plane radar on a round display, for your desk.

- A radar on the 240 × 240 GC9A01 round display at about 25 frames a second: range rings, a compass, and a sweeping beam that makes the aircraft glow as it passes.
- The aircraft around you, live from adsb.fi: small airplanes turned to their track and colored by altitude, with their callsign and altitude, moving smoothly between downloads. Aircraft just outside the range show as marks on the edge.
- A spotlight on the nearest aircraft, one after another, with its name, type, altitude, speed, distance and direction. Emergencies (squawk 7500, 7600 or 7700) blink in red.
- A web app built into the firmware, in English and Italian: the **Radar** tab with the live radar and the nearest aircraft, the **Settings** tab for the location, range (5 to 150 km), units, theme and display, the **Device** tab, and the **WiFi** tab.
- A small HTTP API (`/api/aircraft`, `/api/config`, `/api/settings`) and a Markdown guide for AI agents at `/llms.txt`.
- Wi-Fi setup from the phone, through the setup hotspot and its captive portal, then `gizmo-sky-radar.local` on your network.
- Factory reset with the BOOT button, with a countdown on the display.
