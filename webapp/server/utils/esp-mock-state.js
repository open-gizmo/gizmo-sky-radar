// The Wi-Fi and device mock (its state, the responses and errors of the firmware) comes from the gizmo-common layer: this file adds the settings of the radar and a sky of simulated aircraft
import { EspMockError, getGizmoMockState, restartMockDevice } from 'gizmo-common/web/server/utils/gizmo-mock.js';

// Limits of the firmware (radar_settings.h), the name counted in bytes like the C string on the device
export const RANGE_OPTIONS_KM = [5, 10, 20, 40, 80, 150];
export const UNIT_SYSTEMS = ['aviation', 'metric', 'imperial'];
export const RADAR_THEMES = ['phosphor', 'amber', 'arctic'];
export const BRIGHTNESS = Object.freeze({ min: 5, max: 100 });
export const MAX_ROTATION = 3;
export const MAX_LOCATION_NAME_LENGTH = 24;

// Most aircraft the firmware keeps, and how often it downloads new data (aircraft.h, fetch_schedule.h)
export const MAX_AIRCRAFT = 64;
export const FETCH_INTERVAL_MS = 5000;
const TRAIL_LENGTH = 12;

// Settings of a device right after a reset (the firmware defaults): no location yet
const factoryConfig = () => ({
    hasLocation: false,
    latitude: null,
    longitude: null,
    locationName: '',
    rangeKm: 40,
    units: 'aviation',
    theme: 'phosphor',
    brightness: 80,
    rotation: 0,
    showLabels: true,
    showTrails: true,
    showGroundTraffic: false,
    showSweep: true
});

// Settings of the demo device the dev server starts with (already set up, in Milan)
const demoConfig = () => ({
    ...factoryConfig(),
    hasLocation: true,
    latitude: 45.4642,
    longitude: 9.19,
    locationName: 'Milano'
});

// Get the mock device state: the one of the layer (Wi-Fi, uptime, kept across hot reloads) plus the settings and the flight data of the radar
export const getEspMockState = () => {
    const state = getGizmoMockState();
    state.config ??= demoConfig();
    state.flightStatus ??= 'live'; // 'live', 'loading', 'stale' or 'error', for trying the web app
    return state;
};

// Reset the mock device like the firmware: settings and saved network are cleared, and it restarts into its setup hotspot
export const resetEspMockState = () => {
    const state = getEspMockState();
    state.config = factoryConfig();
    state.savedSsid = '';
    restartMockDevice(state);
    return state;
};

/******************************** Settings ********************************/

// Length of a string in bytes, like strlen() on the device
const byteLength = text => Buffer.byteLength(text, 'utf8');

// Check a number field: missing, or a finite number within the limits
const isNumberInRange = (value, minimum, maximum) => value == null || (typeof value === 'number' && Number.isFinite(value) && value >= minimum && value <= maximum);

// Check a whole number field: missing, or an integer within the limits
const isIntegerInRange = (value, minimum, maximum) => value == null || (Number.isInteger(value) && value >= minimum && value <= maximum);

// The field of a settings body that is wrong, in the order the firmware checks them (null when every field present is valid)
export const findInvalidSettingsField = body => {
    if (!isNumberInRange(body.latitude, -90, 90)) return 'latitude';
    if (!isNumberInRange(body.longitude, -180, 180)) return 'longitude';
    if ((body.latitude == null) !== (body.longitude == null)) return body.latitude == null ? 'latitude' : 'longitude';
    if (body.locationName != null && (typeof body.locationName !== 'string' || byteLength(body.locationName) > MAX_LOCATION_NAME_LENGTH)) return 'locationName';
    if (body.rangeKm != null && !RANGE_OPTIONS_KM.includes(body.rangeKm)) return 'rangeKm';
    if (body.units != null && !UNIT_SYSTEMS.includes(body.units)) return 'units';
    if (body.theme != null && !RADAR_THEMES.includes(body.theme)) return 'theme';
    if (!isIntegerInRange(body.brightness, BRIGHTNESS.min, BRIGHTNESS.max)) return 'brightness';
    if (!isIntegerInRange(body.rotation, 0, MAX_ROTATION)) return 'rotation';
    return ['showLabels', 'showTrails', 'showGroundTraffic', 'showSweep'].find(field => body[field] != null && typeof body[field] !== 'boolean') ?? null;
};

// Apply the fields of a valid settings body (missing fields keep their value; coordinates give the radar its location)
export const applySettings = (config, body) => {
    const next = { ...config };
    if (body.latitude != null && body.longitude != null) {
        next.latitude = body.latitude;
        next.longitude = body.longitude;
        next.hasLocation = true;
    }
    for (const field of ['locationName', 'rangeKm', 'units', 'theme', 'brightness', 'rotation', 'showLabels', 'showTrails', 'showGroundTraffic', 'showSweep']) {
        if (body[field] != null) next[field] = body[field];
    }
    return next;
};

// Check a settings body, throwing the error of the firmware for the first wrong field
export const validateSettings = body => {
    const invalidField = findInvalidSettingsField(body);
    if (invalidField) {
        throw new EspMockError(400, 'invalid_settings_field', invalidField);
    }
};

/******************************** Simulated sky ********************************/

// The aircraft of the simulated sky: where each starts (bearing and distance in km from the radar), its course and its flight
const SIMULATED_FLIGHTS = [
    { hex: '4b1819', callsign: 'SWR2KT', registration: 'HB-JDH', type: 'A20N', description: 'AIRBUS A-320neo', bearing: 200, distance: 18, track: 20, speed: 455, altitude: 35950, rate: 0, category: 'A3' },
    { hex: '4ca762', callsign: 'RYR69QR', registration: 'EI-EFG', type: 'B738', description: 'BOEING 737-800', bearing: 300, distance: 30, track: 349, speed: 160, altitude: 2400, rate: -750, category: 'A3' },
    { hex: '3c6545', callsign: 'DLH1323', registration: 'D-AIJE', type: 'A20N', description: 'AIRBUS A-320neo', bearing: 120, distance: 34, track: 300, speed: 446, altitude: 37950, rate: 0, category: 'A3' },
    { hex: '0100a3', callsign: 'MSR532', registration: 'SU-GCF', type: 'A332', description: 'AIRBUS A-330-200', bearing: 320, distance: 12, track: 4, speed: 228, altitude: 3575, rate: 1800, category: 'A5' },
    { hex: '3c6917', callsign: 'CFG4VR', registration: 'D-AJHW', type: 'E190', description: 'EMBRAER ERJ-190-100', bearing: 30, distance: 16, track: 167, speed: 250, altitude: 6725, rate: -1100, category: 'A3' },
    { hex: '3db145', callsign: 'DGNJP', registration: 'D-GNJP', type: 'DA62', description: 'DIAMOND DA-62', bearing: 80, distance: 9, track: 24, speed: 151, altitude: 1750, rate: 400, category: 'A1' },
    { hex: '320044', callsign: 'POLI118', registration: 'MM81979', type: 'A139', description: 'AGUSTA AW-139', bearing: 160, distance: 6, track: 114, speed: 122, altitude: 1250, rate: 0, category: 'A7' },
    { hex: '4d251d', callsign: 'RYR55YP', registration: '9H-QFA', type: 'B738', description: 'BOEING 737-800', bearing: 250, distance: 7, track: 223, speed: 255, altitude: 8525, rate: -1200, category: 'A3' },
    { hex: '4d2101', callsign: 'WMT3840', registration: '9H-WMT', type: 'A21N', description: 'AIRBUS A-321neo', bearing: 10, distance: 20, track: 316, speed: 416, altitude: 35975, rate: 0, category: 'A3' },
    { hex: 'c0411d', callsign: 'CNK1', registration: 'C-FYRC', type: 'GLF6', description: 'GULFSTREAM G650', bearing: 280, distance: 5, track: 340, speed: 233, altitude: 3575, rate: 2200, category: 'A3' },
    { hex: '39de45', callsign: 'TVF33CH', registration: 'F-HXSF', type: 'A20N', description: 'AIRBUS A-320neo', bearing: 330, distance: 38, track: 103, speed: 490, altitude: 39025, rate: 0, category: 'A3' },
    { hex: '300a3b', callsign: 'ITY2044', registration: 'I-ADVH', type: 'BCS1', description: 'AIRBUS A220-100', bearing: 180, distance: 42, track: 349, speed: 235, altitude: 5750, rate: 1500, category: 'A3' },
    { hex: '4cadbf', callsign: 'RYR3231', registration: 'EI-IGM', type: 'B38M', description: 'BOEING 737 MAX 8', bearing: 60, distance: 26, track: 225, speed: 237, altitude: 7825, rate: -900, category: 'A3' },
    { hex: '3009fb', callsign: 'ICNTK', registration: 'I-CNTK', type: 'TWEN', description: 'TECNAM Twenty-Ten', bearing: 100, distance: 22, track: 80, speed: 94, altitude: 1650, rate: 0, category: 'A1' },
    { hex: '300843', callsign: '', registration: 'I-ZAPA', type: 'C150', description: 'CESSNA 150', bearing: 225, distance: 28, track: 269, speed: 89, altitude: 1575, rate: 0, category: 'A2' },
    { hex: '3c4dc8', callsign: 'DLH9EV', registration: 'D-AIBF', type: 'A319', description: 'AIRBUS A-319', bearing: 300, distance: 26, track: 0, speed: 0, altitude: null, rate: null, category: 'A3', onGround: true },
    { hex: '440cfe', callsign: 'EJU83MQ', registration: 'OE-LVK', type: 'A319', description: 'AIRBUS A-319', bearing: 140, distance: 47, track: 330, speed: 151, altitude: 1975, rate: -896, category: 'A3' },
    { hex: '4ca9e1', callsign: 'RYR8EM', registration: 'EI-DCL', type: 'B738', description: 'BOEING 737-800', bearing: 75, distance: 55, track: 255, speed: 410, altitude: 24000, rate: -1800, category: 'A3' }
];

// How far a simulated aircraft flies before it starts over, in km from the radar (a little past the range, so the ones leaving show on the rim)
const SIMULATION_RADIUS_FACTOR = 1.35;

// Longest step between two points of a trail, in km (an airliner flies about 1.3 km in 5 seconds; a longer step is the simulation starting a flight over)
const MAX_TRAIL_STEP_KM = 3;
const KM_PER_NAUTICAL_MILE = 1.852;
const EARTH_RADIUS_KM = 6371.0088;

// The point reached going a distance along a bearing (geo.cpp)
const movePoint = ({ latitude, longitude }, bearingDegrees, distanceKm) => {
    const toRadians = degrees => degrees * Math.PI / 180;
    const angularDistance = distanceKm / EARTH_RADIUS_KM;
    const bearing = toRadians(bearingDegrees);
    const fromLatitude = toRadians(latitude);
    const toLatitude = Math.asin(Math.sin(fromLatitude) * Math.cos(angularDistance) + Math.cos(fromLatitude) * Math.sin(angularDistance) * Math.cos(bearing));
    const toLongitude = toRadians(longitude) + Math.atan2(Math.sin(bearing) * Math.sin(angularDistance) * Math.cos(fromLatitude), Math.cos(angularDistance) - Math.sin(fromLatitude) * Math.sin(toLatitude));
    return { latitude: toLatitude * 180 / Math.PI, longitude: ((toLongitude * 180 / Math.PI + 540) % 360) - 180 };
};

// Great-circle distance and initial bearing between two points (geo.cpp)
const measure = (from, to) => {
    const toRadians = degrees => degrees * Math.PI / 180;
    const latitudeDelta = toRadians(to.latitude - from.latitude);
    const longitudeDelta = toRadians(to.longitude - from.longitude);
    const a = Math.sin(latitudeDelta / 2) ** 2 + Math.cos(toRadians(from.latitude)) * Math.cos(toRadians(to.latitude)) * Math.sin(longitudeDelta / 2) ** 2;
    const y = Math.sin(longitudeDelta) * Math.cos(toRadians(to.latitude));
    const x = Math.cos(toRadians(from.latitude)) * Math.sin(toRadians(to.latitude)) - Math.sin(toRadians(from.latitude)) * Math.cos(toRadians(to.latitude)) * Math.cos(longitudeDelta);
    return {
        distanceKm: EARTH_RADIUS_KM * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a)),
        bearingDegrees: ((Math.atan2(y, x) * 180 / Math.PI) + 360) % 360
    };
};

// Round a number to some decimals
const round = (value, decimals) => Math.round(value * 10 ** decimals) / 10 ** decimals;

// Where a simulated aircraft is at a time: its start, moved along its track, starting over when it leaves the area
const getSimulatedPosition = ({ flight, center, rangeKm, timeMs }) => {
    // Its start, scaled to the range (the flights are written for 40 km)
    const scale = rangeKm / 40;
    const start = movePoint(center, flight.bearing, flight.distance * scale);
    if (!flight.speed) return start;

    // The distance it flew, over the loop it flies again and again
    const speedKmPerSecond = flight.speed * KM_PER_NAUTICAL_MILE / 3600;
    const loopKm = rangeKm * SIMULATION_RADIUS_FACTOR * 2;
    const flownKm = (speedKmPerSecond * timeMs / 1000 + flight.distance * 7) % loopKm;
    const position = movePoint(start, flight.track, flownKm);

    // Past the area, it comes back in from the other side, on the same track
    if (measure(center, position).distanceKm > rangeKm * SIMULATION_RADIUS_FACTOR) {
        return movePoint(position, flight.track, -loopKm / 2);
    }
    return position;
};

// The aircraft of the simulated sky, like GET /api/aircraft of the firmware: nearest first, each with its trail of the last downloads
export const buildSimulatedAircraft = ({ config, now = Date.now(), limit = MAX_AIRCRAFT, includeTrails = true }) => {
    const center = { latitude: config.latitude, longitude: config.longitude };
    const downloadTime = now - (now % FETCH_INTERVAL_MS); // The data comes from the last download
    return SIMULATED_FLIGHTS
        .filter(flight => config.showGroundTraffic || !flight.onGround)
        .map(flight => {
            const position = getSimulatedPosition({ flight, center, rangeKm: config.rangeKm, timeMs: downloadTime });
            const { distanceKm, bearingDegrees } = measure(center, position);
            const aircraft = {
                hex: flight.hex,
                callsign: flight.callsign || null,
                registration: flight.registration,
                type: flight.type,
                description: flight.description,
                squawk: flight.hex === '4ca762' ? '7700' : '1000',
                category: flight.category,
                latitude: round(position.latitude, 5),
                longitude: round(position.longitude, 5),
                positionAgeSeconds: round((now - downloadTime) / 1000 + 0.8, 1),
                onGround: Boolean(flight.onGround),
                altitudeFt: flight.altitude,
                groundSpeedKt: flight.onGround ? 0 : flight.speed,
                trackDegrees: flight.track,
                verticalRateFpm: flight.rate,
                emergency: flight.hex === '4ca762', // One aircraft squawks 7700, to show how an emergency looks
                distanceKm: round(distanceKm, 2),
                bearingDegrees: round(bearingDegrees, 1),
                inRange: distanceKm <= config.rangeKm,
                missing: false
            };

            // Its earlier positions, one per download, oldest first (cut where the simulation started it over, so the trail never jumps across the radar)
            if (includeTrails) {
                const points = Array.from({ length: TRAIL_LENGTH }, (_, index) => getSimulatedPosition({ flight, center, rangeKm: config.rangeKm, timeMs: downloadTime - (TRAIL_LENGTH - 1 - index) * FETCH_INTERVAL_MS }));
                const lastJump = points.findLastIndex((point, index) => index > 0 && measure(points[index - 1], point).distanceKm > MAX_TRAIL_STEP_KM);
                aircraft.trail = points.slice(Math.max(lastJump, 0)).map(point => [round(point.latitude, 5), round(point.longitude, 5)]);
            }
            return aircraft;
        })
        .sort((first, second) => first.distanceKm - second.distanceKm)
        .slice(0, limit);
};

// The state of the flight data, like the firmware (radar_json.cpp)
export const buildFlightState = state => {
    if (!state.config.hasLocation) {
        return { status: 'no_location', dataAgeSeconds: null, areaTotal: 0, error: null };
    }
    switch (state.flightStatus) {
        case 'loading':
            return { status: 'loading', dataAgeSeconds: null, areaTotal: 0, error: null };
        case 'error':
            return { status: 'error', dataAgeSeconds: null, areaTotal: 0, error: 'No answer from adsb.fi' };
        case 'stale':
            return { status: 'stale', dataAgeSeconds: 185, areaTotal: SIMULATED_FLIGHTS.length, error: 'No answer from adsb.fi' };
        default:
            return { status: 'live', dataAgeSeconds: Math.floor((Date.now() % FETCH_INTERVAL_MS) / 1000), areaTotal: SIMULATED_FLIGHTS.length + 7, error: null };
    }
};

// Build the answer of GET /api/aircraft: the center, the state of the data and the aircraft (none without a location or data)
export const buildAircraftPayload = ({ state, limit = MAX_AIRCRAFT, includeTrails = true }) => {
    const { config } = state;
    const flightState = buildFlightState(state);
    const hasAircraft = config.hasLocation && ['live', 'stale'].includes(flightState.status);
    return {
        center: config.hasLocation ? { latitude: config.latitude, longitude: config.longitude } : null,
        rangeKm: config.rangeKm,
        ...flightState,
        aircraft: hasAircraft ? buildSimulatedAircraft({ config, limit, includeTrails }) : []
    };
};
