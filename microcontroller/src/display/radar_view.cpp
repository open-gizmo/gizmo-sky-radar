#include "radar_view.h"
#include <math.h>
#include <string.h>
#include "units.h"

namespace {
	constexpr float PI_FLOAT = 3.14159265f;
	constexpr float DEG_TO_RAD_FLOAT = PI_FLOAT / 180.0f;

	// The beam: its trail, in degrees behind the leading edge, drawn in slices
	constexpr int SWEEP_TRAIL_DEGREES = 60;
	constexpr int SWEEP_SLICE_DEGREES = 3;

	// How long an aircraft glows after the beam passes it, in degrees of the beam
	constexpr float BLIP_GLOW_DEGREES = 120.0f;

	// Aircraft drawn up to just past the outer ring; farther ones get a marker on the rim
	constexpr float AIRCRAFT_MAX_RADIUS = RADAR_RING_RADIUS + 6;
	constexpr float RIM_MARKER_RADIUS = RADAR_RING_RADIUS + 9;

	// Labels next to the aircraft: how many at most, and their size (two lines of the 6x8 font)
	constexpr int MAX_LABELS = 10;
	constexpr int LABEL_LINE_HEIGHT = 9;

	// The spotlight moves to the next of the nearest aircraft every few seconds
	constexpr uint32_t SPOTLIGHT_PERIOD_MS = 6000;
	constexpr size_t SPOTLIGHT_CANDIDATES = 3;

	// Kilometers in a degree of latitude, and of longitude at the equator
	constexpr float KM_PER_DEGREE_LATITUDE = 110.574f;
	constexpr float KM_PER_DEGREE_LONGITUDE = 111.320f;

	// A point of the screen
	struct ScreenPoint {
		float x;
		float y;
	};

	// A rectangle of the screen, taken by a label
	struct LabelBox {
		int left;
		int top;
		int right;
		int bottom;
	};

	// Turns positions into points of the screen, for the location and range of the radar (an flat projection: exact enough for a few hundred kilometers)
	class RadarProjection {
	public:
		RadarProjection(const RadarSettings &settings)
			: centerLatitude(static_cast<float>(settings.latitude)), centerLongitude(static_cast<float>(settings.longitude)), longitudeScale(cosf(static_cast<float>(settings.latitude) * DEG_TO_RAD_FLOAT)), pixelsPerKm(static_cast<float>(RADAR_RING_RADIUS) / settings.rangeKm) {}

		// The point of a position
		ScreenPoint project(const GeoPoint &position) const {
			float longitudeDelta = static_cast<float>(position.longitude) - centerLongitude;
			if (longitudeDelta > 180.0f) {
				longitudeDelta -= 360.0f;
			} else if (longitudeDelta < -180.0f) {
				longitudeDelta += 360.0f;
			}
			const float eastKm = longitudeDelta * longitudeScale * KM_PER_DEGREE_LONGITUDE;
			const float northKm = (static_cast<float>(position.latitude) - centerLatitude) * KM_PER_DEGREE_LATITUDE;
			return { RADAR_CENTER + eastKm * pixelsPerKm, RADAR_CENTER - northKm * pixelsPerKm };
		}

	private:
		float centerLatitude;
		float centerLongitude;
		float longitudeScale;
		float pixelsPerKm;
	};

	// Distance of a point from the center of the screen
	float getRadius(const ScreenPoint &point) {
		return sqrtf((point.x - RADAR_CENTER) * (point.x - RADAR_CENTER) + (point.y - RADAR_CENTER) * (point.y - RADAR_CENTER));
	}

	// The point at a radius and a bearing (clockwise from north)
	ScreenPoint pointAt(float radius, float bearingDegrees) {
		const float angle = bearingDegrees * DEG_TO_RAD_FLOAT;
		return { RADAR_CENTER + radius * sinf(angle), RADAR_CENTER - radius * cosf(angle) };
	}

	// Where the beam points now, clockwise from north
	float getSweepBearing(uint32_t nowMs) {
		return (nowMs % SWEEP_PERIOD_MS) * 360.0f / SWEEP_PERIOD_MS;
	}

	// Rotate a point of a shape (nose up) by a heading, and move it to a place on the screen
	ScreenPoint placeShapePoint(float x, float y, float headingDegrees, float scale, const ScreenPoint &origin) {
		const float angle = headingDegrees * DEG_TO_RAD_FLOAT;
		const float cosine = cosf(angle);
		const float sine = sinf(angle);
		return { origin.x + (x * cosine - y * sine) * scale, origin.y + (x * sine + y * cosine) * scale };
	}

	// Fill a triangle of a shape
	void fillShapeTriangle(LGFX_Sprite &canvas, const ScreenPoint &origin, float headingDegrees, float scale, const float (&points)[6], uint32_t color) {
		const ScreenPoint a = placeShapePoint(points[0], points[1], headingDegrees, scale, origin);
		const ScreenPoint b = placeShapePoint(points[2], points[3], headingDegrees, scale, origin);
		const ScreenPoint c = placeShapePoint(points[4], points[5], headingDegrees, scale, origin);
		canvas.fillTriangle(lroundf(a.x), lroundf(a.y), lroundf(b.x), lroundf(b.y), lroundf(c.x), lroundf(c.y), color);
	}

	// Draw an airplane seen from above, its nose along its track
	void drawAirplane(LGFX_Sprite &canvas, const ScreenPoint &origin, float trackDegrees, float scale, uint32_t color) {
		constexpr float FUSELAGE[6] = { 0, -7.5f, -1.4f, 5.5f, 1.4f, 5.5f };
		constexpr float WINGS[6] = { 0, -2.5f, -7.0f, 2.2f, 7.0f, 2.2f };
		constexpr float TAIL[6] = { 0, 3.0f, -3.2f, 6.5f, 3.2f, 6.5f };
		fillShapeTriangle(canvas, origin, trackDegrees, scale, FUSELAGE, color);
		fillShapeTriangle(canvas, origin, trackDegrees, scale, WINGS, color);
		fillShapeTriangle(canvas, origin, trackDegrees, scale, TAIL, color);
	}

	// Draw an aircraft without a track: a diamond
	void drawDiamond(LGFX_Sprite &canvas, const ScreenPoint &origin, float size, uint32_t color) {
		const int x = lroundf(origin.x);
		const int y = lroundf(origin.y);
		const int half = lroundf(size);
		canvas.fillTriangle(x, y - half, x - half, y, x + half, y, color);
		canvas.fillTriangle(x, y + half, x - half, y, x + half, y, color);
	}

	// Whether two label boxes overlap
	bool isOverlapping(const LabelBox &first, const LabelBox &second) {
		return first.left < second.right && second.left < first.right && first.top < second.bottom && second.top < first.bottom;
	}

	// Whether a label box stays inside the round glass
	bool isInsideGlass(const LabelBox &box) {
		const int corners[4][2] = { { box.left, box.top }, { box.right, box.top }, { box.left, box.bottom }, { box.right, box.bottom } };
		for (const auto &corner : corners) {
			const float dx = corner[0] - RADAR_CENTER;
			const float dy = corner[1] - RADAR_CENTER;
			if (dx * dx + dy * dy > (RADAR_RING_RADIUS + 2) * (RADAR_RING_RADIUS + 2)) {
				return false;
			}
		}
		return true;
	}

	// The aircraft the spotlight is on: one of the nearest airborne ones inside the range, a new one every few seconds (-1 when there is none)
	int findSpotlightIndex(const AircraftTracker &tracker, const RadarSettings &settings, uint32_t nowMs) {
		int candidates[SPOTLIGHT_CANDIDATES];
		size_t count = 0;
		for (size_t index = 0; index < tracker.count && count < SPOTLIGHT_CANDIDATES; index++) {
			const Aircraft &aircraft = tracker.items[index].aircraft;
			if (!aircraft.isOnGround && aircraft.distanceKm <= settings.rangeKm && !isAircraftMissing(tracker, tracker.items[index])) {
				candidates[count++] = static_cast<int>(index);
			}
		}
		return count == 0 ? -1 : candidates[(nowMs / SPOTLIGHT_PERIOD_MS) % count];
	}

	// Draw the corners of the spotlight around an aircraft
	void drawSpotlightBrackets(LGFX_Sprite &canvas, const ScreenPoint &point, uint32_t color) {
		const int x = lroundf(point.x);
		const int y = lroundf(point.y);
		constexpr int OFFSET = 11;
		constexpr int ARM = 4;
		for (int horizontal = -1; horizontal <= 1; horizontal += 2) {
			for (int vertical = -1; vertical <= 1; vertical += 2) {
				const int cornerX = x + horizontal * OFFSET;
				const int cornerY = y + vertical * OFFSET;
				canvas.drawFastHLine(horizontal < 0 ? cornerX : cornerX - ARM + 1, cornerY, ARM, color);
				canvas.drawFastVLine(cornerX, vertical < 0 ? cornerY : cornerY - ARM + 1, ARM, color);
			}
		}
	}

	// Draw the details of the aircraft in the spotlight, at the bottom of the screen: its name, type and altitude, then its speed, distance and direction
	void drawSpotlightDetails(LGFX_Sprite &canvas, const RadarPalette &palette, const RadarSettings &settings, const Aircraft &aircraft, uint32_t nameColor) {
		char altitude[16];
		char speed[16];
		char distance[16];
		char line[48];
		formatAltitude(altitude, sizeof(altitude), aircraft.hasAltitude, aircraft.isOnGround, aircraft.altitudeFt, settings.units);
		formatDistance(distance, sizeof(distance), aircraft.distanceKm, settings.units);

		// A dark band behind the text, so it reads over the traffic
		canvas.fillRoundRect(RADAR_CENTER - 62, 172, 124, 44, 8, palette.background);
		canvas.drawRoundRect(RADAR_CENTER - 62, 172, 124, 44, 8, palette.grid);

		// The name
		canvas.setTextDatum(textdatum_t::middle_center);
		canvas.setFont(&fonts::FreeSansBold9pt7b);
		canvas.setTextColor(nameColor);
		canvas.drawString(getAircraftDisplayName(aircraft), RADAR_CENTER, 183);

		// The type and the altitude
		canvas.setFont(&fonts::Font0);
		canvas.setTextColor(palette.text);
		snprintf(line, sizeof(line), "%s%s%s", aircraft.typeCode, aircraft.typeCode[0] != '\0' ? "  " : "", altitude);
		canvas.drawString(line, RADAR_CENTER, 197);

		// The speed, the distance and the direction
		canvas.setTextColor(palette.textDim);
		if (aircraft.hasGroundSpeed) {
			formatSpeed(speed, sizeof(speed), aircraft.groundSpeedKt, settings.units);
			snprintf(line, sizeof(line), "%s  %s %s", speed, distance, getCompassPoint(aircraft.bearingDegrees));
		} else {
			snprintf(line, sizeof(line), "%s %s", distance, getCompassPoint(aircraft.bearingDegrees));
		}
		canvas.drawString(line, RADAR_CENTER, 207);
	}

	// Draw the line of status at the top: how many aircraft, or what is wrong with the data
	void drawStatusLine(LGFX_Sprite &canvas, const RadarPalette &palette, const FlightDataState &flightState, size_t inRangeCount, uint32_t nowMs) {
		char text[40];
		uint32_t color = palette.textDim;
		switch (flightState.status) {
			// The first download is on its way: dots that come and go
			case FlightDataStatus::Loading:
			case FlightDataStatus::WaitingForNetwork: {
				const int dots = (nowMs / 400) % 4;
				snprintf(text, sizeof(text), "SCANNING%.*s", dots, "...");
				break;
			}

			// No download has worked yet
			case FlightDataStatus::Error:
				snprintf(text, sizeof(text), "NO DATA - RETRYING");
				color = palette.warning;
				break;

			// The downloads keep failing: say how old the aircraft are
			case FlightDataStatus::Stale:
				snprintf(text, sizeof(text), "DATA %lu MIN OLD", static_cast<unsigned long>(flightState.dataAgeSeconds / 60));
				color = palette.warning;
				break;

			// Live: the aircraft in range
			default:
				if (inRangeCount == 0) {
					snprintf(text, sizeof(text), "CLEAR SKY");
				} else {
					snprintf(text, sizeof(text), "%u AIRCRAFT", static_cast<unsigned>(inRangeCount));
				}
				break;
		}
		canvas.setFont(&fonts::Font0);
		canvas.setTextDatum(textdatum_t::middle_center);
		canvas.setTextColor(color);
		canvas.drawString(text, RADAR_CENTER, 36);
	}

	// Draw the trail of an aircraft: its earlier positions, fading out toward the oldest, up to where it is now
	void drawTrail(LGFX_Sprite &canvas, const RadarProjection &projection, const TrackedAircraft &tracked, const ScreenPoint &current, uint32_t color, uint32_t background) {
		if (tracked.trailCount == 0) {
			return;
		}
		ScreenPoint previous = projection.project(tracked.trail[0]);
		for (uint8_t index = 1; index <= tracked.trailCount; index++) {
			const ScreenPoint next = index < tracked.trailCount ? projection.project(tracked.trail[index]) : current;
			const uint8_t strength = static_cast<uint8_t>(40 + 130 * index / tracked.trailCount);
			if (getRadius(previous) <= AIRCRAFT_MAX_RADIUS && getRadius(next) <= AIRCRAFT_MAX_RADIUS) {
				canvas.drawLine(lroundf(previous.x), lroundf(previous.y), lroundf(next.x), lroundf(next.y), mixColors(background, color, strength));
			}
			previous = next;
		}
	}

	// Draw the label of an aircraft (its name, then its altitude in hundreds with an arrow when it climbs or descends) where it fits, and remember its box
	void drawLabel(LGFX_Sprite &canvas, const RadarPalette &palette, const RadarSettings &settings, const Aircraft &aircraft, const ScreenPoint &point, uint32_t color, LabelBox *boxes, int &boxCount) {
		if (boxCount >= MAX_LABELS) {
			return;
		}

		// The two lines
		const char *name = getAircraftDisplayName(aircraft);
		char altitude[12];
		formatShortAltitude(altitude, sizeof(altitude), aircraft.hasAltitude, aircraft.isOnGround, aircraft.altitudeFt, settings.units);
		const bool hasTrend = aircraft.hasVerticalRate && abs(aircraft.verticalRateFpm) >= 300 && !aircraft.isOnGround;
		const size_t nameLength = strlen(name);
		const size_t altitudeLength = strlen(altitude) + (hasTrend ? 2 : 0);
		const int width = static_cast<int>(nameLength > altitudeLength ? nameLength : altitudeLength) * 6;
		const int height = LABEL_LINE_HEIGHT * 2;

		// Right of the aircraft, else left of it, else above or below: the first place that is free and inside the glass
		const int x = lroundf(point.x);
		const int y = lroundf(point.y);
		const LabelBox options[4] = {
			{ x + 9, y - 10, x + 9 + width, y - 10 + height },
			{ x - 9 - width, y - 10, x - 9, y - 10 + height },
			{ x - width / 2, y - 12 - height, x - width / 2 + width, y - 12 },
			{ x - width / 2, y + 11, x - width / 2 + width, y + 11 + height }
		};
		for (const LabelBox &box : options) {
			bool isFree = isInsideGlass(box);
			for (int index = 0; index < boxCount && isFree; index++) {
				isFree = !isOverlapping(box, boxes[index]);
			}
			if (!isFree) {
				continue;
			}

			// Draw it there
			canvas.setFont(&fonts::Font0);
			canvas.setTextDatum(textdatum_t::top_left);
			canvas.setTextColor(color);
			canvas.drawString(name, box.left, box.top);
			canvas.setTextColor(palette.textDim);
			canvas.drawString(altitude, box.left, box.top + LABEL_LINE_HEIGHT);

			// The arrow of a climb or a descent
			if (hasTrend) {
				const int arrowX = box.left + static_cast<int>(strlen(altitude)) * 6 + 4;
				const int arrowY = box.top + LABEL_LINE_HEIGHT + 3;
				if (aircraft.verticalRateFpm > 0) {
					canvas.fillTriangle(arrowX, arrowY - 3, arrowX - 3, arrowY + 2, arrowX + 3, arrowY + 2, palette.textDim);
				} else {
					canvas.fillTriangle(arrowX, arrowY + 3, arrowX - 3, arrowY - 2, arrowX + 3, arrowY - 2, palette.textDim);
				}
			}
			boxes[boxCount++] = box;
			return;
		}
	}
}

// Paint the background that never changes between frames
void drawRadarBackground(LGFX_Sprite &canvas, const RadarPalette &palette) {
	// A glow toward the center: rings from the edge color to the center color
	canvas.fillScreen(static_cast<uint32_t>(0x000000));
	for (int radius = RADAR_EDGE_RADIUS + 1; radius > 0; radius -= 2) {
		const uint8_t amount = static_cast<uint8_t>(255 - (radius * radius * 255) / ((RADAR_EDGE_RADIUS + 1) * (RADAR_EDGE_RADIUS + 1)));
		canvas.fillCircle(RADAR_CENTER, RADAR_CENTER, radius, mixColors(palette.backgroundEdge, palette.background, amount));
	}
}

// Draw the beam, the rings and the compass
void drawRadarScope(LGFX_Sprite &canvas, const RadarPalette &palette, const RadarSettings &settings, uint32_t nowMs) {
	// The beam: slices fading out behind its leading edge, then the edge itself
	if (settings.showSweep) {
		const float sweepBearing = getSweepBearing(nowMs);
		for (int offset = SWEEP_TRAIL_DEGREES; offset > 0; offset -= SWEEP_SLICE_DEGREES) {
			// A thin wedge (a triangle from the center: the arc of a few degrees is nearly straight), brighter toward the edge of the beam
			const uint8_t strength = static_cast<uint8_t>(((SWEEP_TRAIL_DEGREES - offset) * (SWEEP_TRAIL_DEGREES - offset) * 110) / (SWEEP_TRAIL_DEGREES * SWEEP_TRAIL_DEGREES));
			const ScreenPoint start = pointAt(RADAR_RING_RADIUS, sweepBearing - offset);
			const ScreenPoint end = pointAt(RADAR_RING_RADIUS, sweepBearing - offset + SWEEP_SLICE_DEGREES + 0.6f);
			canvas.fillTriangle(RADAR_CENTER, RADAR_CENTER, lroundf(start.x), lroundf(start.y), lroundf(end.x), lroundf(end.y), mixColors(palette.background, palette.sweep, strength));
		}
		// The leading edge, two pixels wide
		const ScreenPoint edge = pointAt(RADAR_RING_RADIUS, sweepBearing);
		const ScreenPoint edgeBeside = pointAt(RADAR_RING_RADIUS, sweepBearing - 0.8f);
		canvas.drawLine(RADAR_CENTER, RADAR_CENTER, lroundf(edge.x), lroundf(edge.y), palette.sweep);
		canvas.drawLine(RADAR_CENTER, RADAR_CENTER, lroundf(edgeBeside.x), lroundf(edgeBeside.y), mixColors(palette.background, palette.sweep, 170));
	}

	// The range rings, and the crosshair
	canvas.drawCircle(RADAR_CENTER, RADAR_CENTER, RADAR_RING_RADIUS / 3, palette.grid);
	canvas.drawCircle(RADAR_CENTER, RADAR_CENTER, RADAR_RING_RADIUS * 2 / 3, palette.grid);
	canvas.drawCircle(RADAR_CENTER, RADAR_CENTER, RADAR_RING_RADIUS, palette.gridBright);
	for (int offset = 6; offset < RADAR_RING_RADIUS; offset += 6) {
		canvas.drawPixel(RADAR_CENTER + offset, RADAR_CENTER, palette.grid);
		canvas.drawPixel(RADAR_CENTER - offset, RADAR_CENTER, palette.grid);
		canvas.drawPixel(RADAR_CENTER, RADAR_CENTER + offset, palette.grid);
		canvas.drawPixel(RADAR_CENTER, RADAR_CENTER - offset, palette.grid);
	}

	// The ticks of the bezel: every 10 degrees, longer every 30
	for (int bearing = 0; bearing < 360; bearing += 10) {
		const bool isMajor = bearing % 30 == 0;
		const ScreenPoint inner = pointAt(RADAR_RING_RADIUS + 2, bearing);
		const ScreenPoint outer = pointAt(RADAR_RING_RADIUS + (isMajor ? 6 : 4), bearing);
		canvas.drawLine(lroundf(inner.x), lroundf(inner.y), lroundf(outer.x), lroundf(outer.y), isMajor ? palette.gridBright : palette.grid);
	}

	// The compass points, outside the ring
	constexpr const char *CARDINALS[] = { "N", "E", "S", "W" };
	canvas.setFont(&fonts::Font2);
	canvas.setTextDatum(textdatum_t::middle_center);
	for (int index = 0; index < 4; index++) {
		const ScreenPoint point = pointAt(RADAR_RING_RADIUS + 10.5f, index * 90.0f);
		canvas.fillCircle(lroundf(point.x), lroundf(point.y), 7, palette.backgroundEdge);
		canvas.setTextColor(index == 0 ? palette.accent : palette.text);
		canvas.drawString(CARDINALS[index], lroundf(point.x), lroundf(point.y) + 1);
	}

	// The range of the outer ring, inside it just below east (clear of the status line and the spotlight)
	char range[16];
	formatRange(range, sizeof(range), settings.rangeKm, settings.units);
	canvas.setFont(&fonts::Font0);
	canvas.setTextColor(palette.textDim);
	const ScreenPoint labelPoint = pointAt(RADAR_RING_RADIUS - 16, 101);
	canvas.drawString(range, lroundf(labelPoint.x), lroundf(labelPoint.y));
}

// Draw the aircraft
void drawRadarTraffic(LGFX_Sprite &canvas, const RadarPalette &palette, const RadarSettings &settings, const AircraftTracker &tracker, const FlightDataState &flightState, uint32_t nowMs) {
	const RadarProjection projection(settings);
	const float sweepBearing = getSweepBearing(nowMs);
	const int spotlightIndex = findSpotlightIndex(tracker, settings, nowMs);
	const bool isBlinkOn = (nowMs / 300) % 2 == 0;
	ScreenPoint points[MAX_AIRCRAFT];
	bool isOnScreen[MAX_AIRCRAFT];
	size_t inRangeCount = 0;

	// Where each aircraft is now, moved on from its last position; the ones past the edge get a marker on the rim
	for (size_t index = 0; index < tracker.count; index++) {
		const TrackedAircraft &tracked = tracker.items[index];
		points[index] = projection.project(getEstimatedPosition(tracked, nowMs));
		isOnScreen[index] = getRadius(points[index]) <= AIRCRAFT_MAX_RADIUS;
		if (tracked.aircraft.distanceKm <= settings.rangeKm) {
			inRangeCount++;
		}
		if (!isOnScreen[index]) {
			const float bearing = atan2f(points[index].x - RADAR_CENTER, RADAR_CENTER - points[index].y) / DEG_TO_RAD_FLOAT;
			const ScreenPoint tip = pointAt(RIM_MARKER_RADIUS - 3, bearing);
			const ScreenPoint left = pointAt(RIM_MARKER_RADIUS + 2, bearing - 2.5f);
			const ScreenPoint right = pointAt(RIM_MARKER_RADIUS + 2, bearing + 2.5f);
			const uint32_t color = getAltitudeColor(tracked.aircraft.hasAltitude, tracked.aircraft.isOnGround, tracked.aircraft.altitudeFt);
			canvas.fillTriangle(lroundf(tip.x), lroundf(tip.y), lroundf(left.x), lroundf(left.y), lroundf(right.x), lroundf(right.y), mixColors(palette.backgroundEdge, color, 170));
		}
	}

	// The trails, under every aircraft
	if (settings.showTrails) {
		for (size_t index = 0; index < tracker.count; index++) {
			if (isOnScreen[index]) {
				const Aircraft &aircraft = tracker.items[index].aircraft;
				drawTrail(canvas, projection, tracker.items[index], points[index], getAltitudeColor(aircraft.hasAltitude, aircraft.isOnGround, aircraft.altitudeFt), palette.background);
			}
		}
	}

	// The aircraft, farthest first so the nearest are on top; they glow for a moment after the beam passes them
	for (size_t position = tracker.count; position > 0; position--) {
		const size_t index = position - 1;
		if (!isOnScreen[index]) {
			continue;
		}
		const TrackedAircraft &tracked = tracker.items[index];
		const Aircraft &aircraft = tracked.aircraft;
		uint32_t color = getAltitudeColor(aircraft.hasAltitude, aircraft.isOnGround, aircraft.altitudeFt);
		if (isAircraftMissing(tracker, tracked)) {
			color = mixColors(palette.background, color, 120); // Not in the last download: dimmed
		}

		// The glow of the beam
		if (settings.showSweep) {
			const float bearing = atan2f(points[index].x - RADAR_CENTER, RADAR_CENTER - points[index].y) / DEG_TO_RAD_FLOAT;
			float sinceBeam = sweepBearing - bearing;
			while (sinceBeam < 0) {
				sinceBeam += 360.0f;
			}
			if (sinceBeam < BLIP_GLOW_DEGREES) {
				const uint8_t strength = static_cast<uint8_t>(90.0f * (1.0f - sinceBeam / BLIP_GLOW_DEGREES));
				canvas.fillCircle(lroundf(points[index].x), lroundf(points[index].y), 9, mixColors(palette.background, color, strength));
			}
		}

		// An emergency blinks a red ring
		if (aircraft.isEmergency && isBlinkOn) {
			canvas.drawCircle(lroundf(points[index].x), lroundf(points[index].y), 11, EMERGENCY_COLOR);
			canvas.drawCircle(lroundf(points[index].x), lroundf(points[index].y), 12, EMERGENCY_COLOR);
		}

		// The aircraft, turned to its track (a diamond without one); smaller on the ground
		const float scale = aircraft.isOnGround ? 0.7f : (static_cast<int>(index) == spotlightIndex ? 1.05f : 0.85f);
		if (aircraft.hasTrack) {
			drawAirplane(canvas, points[index], aircraft.trackDegrees, scale, color);
		} else {
			drawDiamond(canvas, points[index], 4 * scale, color);
		}
	}

	// The labels, nearest first so they get the best places (not on the ground, where they would crowd an airport)
	if (settings.showLabels) {
		LabelBox boxes[MAX_LABELS];
		int boxCount = 0;
		if (spotlightIndex >= 0) {
			boxes[boxCount++] = { RADAR_CENTER - 62, 172, RADAR_CENTER + 62, 216 }; // The details of the spotlight
		}
		boxes[boxCount++] = { RADAR_CENTER - 40, 30, RADAR_CENTER + 40, 42 }; // The line of status
		for (size_t index = 0; index < tracker.count; index++) {
			const Aircraft &aircraft = tracker.items[index].aircraft;
			if (isOnScreen[index] && !aircraft.isOnGround && static_cast<int>(index) != spotlightIndex) {
				drawLabel(canvas, palette, settings, aircraft, points[index], aircraft.isEmergency ? EMERGENCY_COLOR : palette.text, boxes, boxCount);
			}
		}
	}

	// The home of the radar
	canvas.fillCircle(RADAR_CENTER, RADAR_CENTER, 2, palette.accent);
	canvas.drawCircle(RADAR_CENTER, RADAR_CENTER, 5, palette.gridBright);

	// The spotlight: its corners, and its details at the bottom
	if (spotlightIndex >= 0) {
		const Aircraft &aircraft = tracker.items[spotlightIndex].aircraft;
		drawSpotlightBrackets(canvas, points[spotlightIndex], palette.accent);
		drawSpotlightDetails(canvas, palette, settings, aircraft, aircraft.isEmergency ? EMERGENCY_COLOR : getAltitudeColor(aircraft.hasAltitude, aircraft.isOnGround, aircraft.altitudeFt));
	}

	// The line of status
	drawStatusLine(canvas, palette, flightState, inRangeCount, nowMs);
}
