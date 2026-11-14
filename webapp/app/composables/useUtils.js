// Numbers, colors and geography of the radar, the same as on the display (units.cpp, theme.cpp and geo.cpp of the firmware)

// Ranges the radar can show (its outer ring), in kilometers (radar_settings.h)
export const RANGE_OPTIONS_KM = [5, 10, 20, 40, 80, 150];

// Units, themes and rotations of the firmware, in the order of the pickers
export const UNIT_SYSTEMS = ['aviation', 'metric', 'imperial'];
export const RADAR_THEMES = ['phosphor', 'amber', 'arctic'];
export const ROTATIONS = [0, 1, 2, 3];

// Brightness limits of the firmware, in percent
export const BRIGHTNESS_MIN = 5;
export const BRIGHTNESS_MAX = 100;

// Largest length of the name of the location, in bytes like the C string on the device
export const LOCATION_NAME_MAX_LENGTH = 24;

const KM_PER_NAUTICAL_MILE = 1.852;
const KM_PER_MILE = 1.609344;
const METERS_PER_FOOT = 0.3048;
const EARTH_RADIUS_KM = 6371.0088;

// Colors of the themes, like the display (theme.cpp)
export const THEME_PALETTES = {
    phosphor: { background: '#04170b', backgroundEdge: '#000402', grid: '#0f4a24', gridBright: '#1e8443', text: '#b4ffd0', textDim: '#52a673', sweep: '#3cff82', accent: '#f0fff5' },
    amber: { background: '#170c02', backgroundEdge: '#040100', grid: '#553108', gridBright: '#96590e', text: '#ffdda8', textDim: '#b27c34', sweep: '#ffae1f', accent: '#fff4e0' },
    arctic: { background: '#061527', backgroundEdge: '#01050c', grid: '#14395a', gridBright: '#2667a0', text: '#d4f1ff', textDim: '#6fa5c8', sweep: '#52d9ff', accent: '#ffffff' }
};

// Bands of altitude and their colors, like the display (the legend of the radar page lists them)
export const ALTITUDE_BANDS = [
    { maxFt: 2000, color: '#ff5e4d' },
    { maxFt: 10000, color: '#ffa43d' },
    { maxFt: 20000, color: '#ffe45a' },
    { maxFt: 30000, color: '#86ff5c' },
    { maxFt: 40000, color: '#4fd8ff' },
    { maxFt: Infinity, color: '#c58cff' }
];
export const GROUND_COLOR = '#9a9a9a';
export const EMERGENCY_COLOR = '#ff2d2d';

// Color of an aircraft from its altitude
export const getAltitudeColor = ({ altitudeFt, onGround }) => {
    if (onGround) return GROUND_COLOR;
    if (altitudeFt == null) return '#dddddd';
    return ALTITUDE_BANDS.find(band => altitudeFt < band.maxFt).color;
};

// Format a number in the language of the app
const formatNumber = (value, locale, decimals = 0) => new Intl.NumberFormat(locale, {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals
}).format(value);

// A distance in kilometers, in the unit of the system, with the name of the unit
const convertDistance = (distanceKm, units) => {
    if (units === 'metric') return { value: distanceKm, unit: 'km' };
    if (units === 'imperial') return { value: distanceKm / KM_PER_MILE, unit: 'mi' };
    return { value: distanceKm / KM_PER_NAUTICAL_MILE, unit: 'nm' };
};

// Altitude: flight levels from 18,000 ft in aviation units, feet or meters otherwise, "GND" on the ground, "—" without one
export const formatAltitude = ({ altitudeFt, onGround, units, locale }) => {
    if (onGround) return 'GND';
    if (altitudeFt == null) return '—';
    if (units === 'metric') return `${formatNumber(Math.round(altitudeFt * METERS_PER_FOOT), locale)} m`;
    if (units === 'aviation' && altitudeFt >= 18000) return `FL${String(Math.round(altitudeFt / 100)).padStart(3, '0')}`;
    return `${formatNumber(altitudeFt, locale)} ft`;
};

// Distance, with a decimal under 10
export const formatDistance = ({ distanceKm, units, locale }) => {
    const { value, unit } = convertDistance(distanceKm, units);
    return `${formatNumber(value, locale, value < 10 ? 1 : 0)} ${unit}`;
};

// Range of the outer ring, without decimals when it's a whole number
export const formatRange = ({ rangeKm, units, locale }) => {
    const { value, unit } = convertDistance(rangeKm, units);
    const isWhole = value >= 10 || Math.abs(value - Math.round(value)) < 0.05;
    return `${formatNumber(value, locale, isWhole ? 0 : 1)} ${unit}`;
};

// Speed
export const formatSpeed = ({ speedKt, units, locale }) => {
    if (speedKt == null) return '—';
    if (units === 'metric') return `${formatNumber(Math.round(speedKt * KM_PER_NAUTICAL_MILE), locale)} km/h`;
    if (units === 'imperial') return `${formatNumber(Math.round(speedKt * KM_PER_NAUTICAL_MILE / KM_PER_MILE), locale)} mph`;
    return `${formatNumber(Math.round(speedKt), locale)} kt`;
};

// Vertical rate: feet per minute (meters per second in metric units), with its sign
export const formatVerticalRate = ({ verticalRateFpm, units, locale }) => {
    if (verticalRateFpm == null) return '—';
    const sign = verticalRateFpm > 0 ? '+' : '';
    if (units === 'metric') return `${sign}${formatNumber(verticalRateFpm * METERS_PER_FOOT / 60, locale, 1)} m/s`;
    return `${sign}${formatNumber(verticalRateFpm, locale)} ft/min`;
};

// Compass direction of a bearing: N, NE, E, SE, S, SW, W or NW
export const getCompassPoint = bearingDegrees => {
    const points = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
    const bearing = ((bearingDegrees % 360) + 360) % 360;
    return points[Math.floor((bearing + 22.5) / 45) % 8];
};

// Name to show for an aircraft: its callsign, else its registration, else its ICAO address
export const getAircraftName = aircraft => aircraft.callsign || aircraft.registration || aircraft.hex.toUpperCase();

// Read coordinates typed or pasted as "45.4642, 9.19" (the way maps copy them), "45.4642 9.19" or "45.4642;9.19" (null when they aren't coordinates)
export const parseCoordinates = text => {
    const match = String(text ?? '').trim().match(/^(-?\d{1,2}(?:\.\d+)?)\s*[,;\s]\s*(-?\d{1,3}(?:\.\d+)?)$/);
    if (!match) return null;
    const latitude = Number(match[1]);
    const longitude = Number(match[2]);
    if (Math.abs(latitude) > 90 || Math.abs(longitude) > 180) return null;
    return { latitude, longitude };
};

// Write coordinates for the location field, with 5 decimals (about a meter)
export const formatCoordinates = ({ latitude, longitude }) => {
    if (latitude == null || longitude == null) return '';
    return `${Number(latitude).toFixed(5)}, ${Number(longitude).toFixed(5)}`;
};

// The point reached going a distance along a bearing (to move an aircraft on along its track between two reads, like the display)
export const movePoint = ({ latitude, longitude }, bearingDegrees, distanceKm) => {
    const toRadians = degrees => degrees * Math.PI / 180;
    const angularDistance = distanceKm / EARTH_RADIUS_KM;
    const bearing = toRadians(bearingDegrees);
    const fromLatitude = toRadians(latitude);
    const toLatitude = Math.asin(Math.sin(fromLatitude) * Math.cos(angularDistance) + Math.cos(fromLatitude) * Math.sin(angularDistance) * Math.cos(bearing));
    const toLongitude = toRadians(longitude) + Math.atan2(Math.sin(bearing) * Math.sin(angularDistance) * Math.cos(fromLatitude), Math.cos(angularDistance) - Math.sin(fromLatitude) * Math.sin(toLatitude));
    return { latitude: toLatitude * 180 / Math.PI, longitude: ((toLongitude * 180 / Math.PI + 540) % 360) - 180 };
};

// Longest time an aircraft is moved on from its last position, in seconds (aircraft_tracker.h)
const MAX_DEAD_RECKONING_SECONDS = 45;

// Where an aircraft is estimated to be, some seconds after its position was true (its last position on the ground or without a course)
export const estimatePosition = (aircraft, ageSeconds) => {
    const position = { latitude: aircraft.latitude, longitude: aircraft.longitude };
    if (aircraft.onGround || aircraft.groundSpeedKt == null || aircraft.trackDegrees == null || aircraft.groundSpeedKt <= 0) return position;
    const seconds = Math.min(Math.max(ageSeconds, 0), MAX_DEAD_RECKONING_SECONDS);
    return movePoint(position, aircraft.trackDegrees, aircraft.groundSpeedKt * KM_PER_NAUTICAL_MILE * seconds / 3600);
};

// Turn a position into a point of the radar, in units of its range (the center is 0,0, the outer ring has radius 1, north is up), like the display
export const projectPosition = ({ latitude, longitude }, center, rangeKm) => {
    let longitudeDelta = longitude - center.longitude;
    if (longitudeDelta > 180) longitudeDelta -= 360;
    if (longitudeDelta < -180) longitudeDelta += 360;
    const eastKm = longitudeDelta * Math.cos(center.latitude * Math.PI / 180) * 111.32;
    const northKm = (latitude - center.latitude) * 110.574;
    return { x: eastKm / rangeKm, y: -northKm / rangeKm };
};

// Link to the aircraft on the adsb.fi map
export const buildTrackerUrl = hex => `https://globe.adsb.fi/?icao=${encodeURIComponent(hex)}`;
