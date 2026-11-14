#include "geo.h"
#include <math.h>

namespace {
	constexpr double PI_VALUE = 3.14159265358979323846;

	// Degrees to radians, and back
	double toRadians(double degrees) {
		return degrees * PI_VALUE / 180.0;
	}
	double toDegrees(double radians) {
		return radians * 180.0 / PI_VALUE;
	}
}

// Great-circle distance between two points (haversine, accurate at the few kilometers of a radar)
double getDistanceKm(const GeoPoint &from, const GeoPoint &to) {
	const double latitudeDelta = toRadians(to.latitude - from.latitude);
	const double longitudeDelta = toRadians(to.longitude - from.longitude);
	const double a = sin(latitudeDelta / 2) * sin(latitudeDelta / 2) + cos(toRadians(from.latitude)) * cos(toRadians(to.latitude)) * sin(longitudeDelta / 2) * sin(longitudeDelta / 2);
	return EARTH_RADIUS_KM * 2 * atan2(sqrt(a), sqrt(1 - a));
}

// Initial bearing from one point to another, clockwise from true north
double getBearingDegrees(const GeoPoint &from, const GeoPoint &to) {
	const double fromLatitude = toRadians(from.latitude);
	const double toLatitude = toRadians(to.latitude);
	const double longitudeDelta = toRadians(to.longitude - from.longitude);
	const double y = sin(longitudeDelta) * cos(toLatitude);
	const double x = cos(fromLatitude) * sin(toLatitude) - sin(fromLatitude) * cos(toLatitude) * cos(longitudeDelta);
	return normalizeDegrees(toDegrees(atan2(y, x)));
}

// The point reached going a distance along a bearing
GeoPoint movePoint(const GeoPoint &from, double bearingDegrees, double distanceKm) {
	const double angularDistance = distanceKm / EARTH_RADIUS_KM;
	const double bearing = toRadians(bearingDegrees);
	const double fromLatitude = toRadians(from.latitude);
	const double fromLongitude = toRadians(from.longitude);
	const double toLatitude = asin(sin(fromLatitude) * cos(angularDistance) + cos(fromLatitude) * sin(angularDistance) * cos(bearing));
	const double toLongitude = fromLongitude + atan2(sin(bearing) * sin(angularDistance) * cos(fromLatitude), cos(angularDistance) - sin(fromLatitude) * sin(toLatitude));

	// Keep the longitude within -180 to 180 (a flight across the date line)
	double longitude = toDegrees(toLongitude);
	while (longitude > 180) {
		longitude -= 360;
	}
	while (longitude < -180) {
		longitude += 360;
	}
	return { toDegrees(toLatitude), longitude };
}

// Bring an angle to 0 to 360 degrees
double normalizeDegrees(double degrees) {
	double result = fmod(degrees, 360.0);
	if (result < 0) {
		result += 360.0;
	}
	return result;
}
