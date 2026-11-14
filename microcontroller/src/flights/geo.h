#ifndef GEO_H
#define GEO_H

// Geography of the radar: distances, bearings and moving a position along a course, kept free of hardware headers so the native tests can use it

// Mean radius of the Earth, in kilometers
constexpr double EARTH_RADIUS_KM = 6371.0088;

// One nautical mile, in kilometers (the unit of the flight data: speeds in knots, the query radius in nautical miles)
constexpr double NAUTICAL_MILE_KM = 1.852;

// A point on the Earth, in degrees
struct GeoPoint {
	double latitude;
	double longitude;
};

// Great-circle distance between two points, in kilometers
double getDistanceKm(const GeoPoint &from, const GeoPoint &to);

// Initial bearing from one point to another, in degrees clockwise from true north (0 to 360)
double getBearingDegrees(const GeoPoint &from, const GeoPoint &to);

// The point reached going a distance along a bearing (for dead reckoning: where an aircraft is now, from its last position, speed and track)
GeoPoint movePoint(const GeoPoint &from, double bearingDegrees, double distanceKm);

// Bring an angle to 0 to 360 degrees
double normalizeDegrees(double degrees);

#endif
