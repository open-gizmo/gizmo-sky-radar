import { IncomingMessage, ServerResponse } from 'node:http';
import { Socket } from 'node:net';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { createEvent } from 'h3';

// Stub the Nitro globals the routes of this app use before the handler modules load (the mock of the gizmo-common layer imports its h3 functions, so it runs on real h3 events)
vi.hoisted(() => {
    globalThis.defineEventHandler = handler => handler;
    globalThis.defineNitroPlugin = plugin => plugin;
    globalThis.useAppConfig = () => ({ gizmo: { name: 'Gizmo Sky Radar', hostname: 'gizmo-sky-radar' } });
});

// Load the handlers of the mock device API
const { default: getConfig } = await import('../../server/api/config/index.get.js');
const { default: postSettings } = await import('../../server/api/settings/index.post.js');
const { default: postResetSettings } = await import('../../server/api/reset-settings/index.post.js');
const { default: getAircraft } = await import('../../server/api/aircraft/index.get.js');
const { buildSimulatedAircraft, getEspMockState } = await import('../../server/utils/esp-mock-state.js');
const { default: nameMockDevice } = await import('../../server/plugins/gizmo-mock.js');
const { buildDevicePayload, resetGizmoMockState } = await import('gizmo-common/web/server/utils/gizmo-mock.js');

// Build a real h3 request event, with a JSON body for a POST
const buildEvent = ({ method = 'GET', url = '/', body } = {}) => {
    const request = new IncomingMessage(new Socket());
    request.method = method;
    request.url = url;
    request.headers = { 'content-type': 'application/json' };
    if (body !== undefined) {
        request.rawBody = Buffer.from(typeof body === 'string' ? body : JSON.stringify(body));
    }
    return createEvent(request, new ServerResponse(request));
};

// Call a handler and return its answer with the status code it set
const callHandler = async (handler, options) => {
    const event = buildEvent(options);
    const response = await handler(event);
    return { statusCode: event.node.res.statusCode, response };
};

// Start every test from the demo device the dev server boots with (in Milan)
beforeEach(() => {
    resetGizmoMockState();
});

// GET /api/config: the settings, like the firmware
describe('mock settings', () => {
    // The demo radar is set up in Milan, with the firmware defaults otherwise
    it('reads the settings of the demo radar', async () => {
        const { statusCode, response } = await callHandler(getConfig);
        expect(statusCode).toBe(200);
        expect(response.errorId).toBeNull();
        expect(response.data).toMatchObject({ hasLocation: true, latitude: 45.4642, longitude: 9.19, locationName: 'Milano', rangeKm: 40, units: 'aviation', theme: 'phosphor', brightness: 80, rotation: 0, showSweep: true });
    });

    // The device card of the layer shows the name and the address of the radar
    it('names the mock device like the radar', async () => {
        nameMockDevice();
        const payload = buildDevicePayload({ state: getEspMockState() });
        expect(payload.name).toBe('Gizmo Sky Radar');
        expect(payload.hostname).toBe('gizmo-sky-radar.local');
    });

    // The fields sent change, the others stay, and the saved settings come back
    it('saves the fields sent', async () => {
        const { statusCode, response } = await callHandler(postSettings, { method: 'POST', body: { latitude: 51.47, longitude: -0.4543, rangeKm: 80, theme: 'amber', rotation: 2 } });
        expect(statusCode).toBe(200);
        expect(response.data).toMatchObject({ latitude: 51.47, longitude: -0.4543, rangeKm: 80, theme: 'amber', rotation: 2, units: 'aviation', locationName: 'Milano' });
        expect(getEspMockState().config.rangeKm).toBe(80);
    });

    // Every wrong field is refused with the error of the firmware, naming it
    it.each([
        [{ latitude: 91, longitude: 9 }, 'latitude'],
        [{ latitude: 45 }, 'longitude'],
        [{ longitude: '9' }, 'longitude'],
        [{ locationName: 'A name much longer than the field' }, 'locationName'],
        [{ rangeKm: 42 }, 'rangeKm'],
        [{ units: 'parsecs' }, 'units'],
        [{ theme: 'pink' }, 'theme'],
        [{ brightness: 4 }, 'brightness'],
        [{ brightness: 50.5 }, 'brightness'],
        [{ rotation: 4 }, 'rotation'],
        [{ showSweep: 1 }, 'showSweep']
    ])('refuses %o', async (body, field) => {
        const { statusCode, response } = await callHandler(postSettings, { method: 'POST', body });
        expect(statusCode).toBe(400);
        expect(response).toEqual({ errorId: 'invalid_settings_field', errorMessage: field, data: null });
    });

    // A body that isn't JSON is refused like the firmware
    it('refuses invalid JSON', async () => {
        const { statusCode, response } = await callHandler(postSettings, { method: 'POST', body: '{not json' });
        expect(statusCode).toBe(400);
        expect(response.errorId).toBe('invalid_json');
    });

    // The reset forgets the location, the settings and the network, then restarts into the setup hotspot
    it('resets the radar', async () => {
        vi.useFakeTimers();
        const { statusCode } = await callHandler(postResetSettings, { method: 'POST' });
        expect(statusCode).toBe(200);
        const state = getEspMockState();
        expect(state.config).toMatchObject({ hasLocation: false, latitude: null, locationName: '' });
        expect(state.savedSsid).toBe('');
        expect(state.isAccessPointActive).toBe(true);
        vi.useRealTimers();
    });
});

// GET /api/aircraft: the simulated sky, like the firmware
describe('mock aircraft', () => {
    // The aircraft come nearest first, with the fields of the firmware and their trails
    it('lists the aircraft nearest first', async () => {
        const { statusCode, response } = await callHandler(getAircraft);
        expect(statusCode).toBe(200);
        const { data } = response;
        expect(data).toMatchObject({ center: { latitude: 45.4642, longitude: 9.19 }, rangeKm: 40, status: 'live', error: null });
        expect(data.aircraft.length).toBeGreaterThan(10);
        const distances = data.aircraft.map(aircraft => aircraft.distanceKm);
        expect(distances).toEqual([...distances].sort((first, second) => first - second));
        expect(Object.keys(data.aircraft[0])).toEqual(expect.arrayContaining(['hex', 'callsign', 'registration', 'type', 'latitude', 'longitude', 'positionAgeSeconds', 'onGround', 'altitudeFt', 'groundSpeedKt', 'trackDegrees', 'verticalRateFpm', 'emergency', 'distanceKm', 'bearingDegrees', 'inRange', 'missing', 'trail']));
        expect(data.aircraft.every(aircraft => aircraft.inRange === aircraft.distanceKm <= 40)).toBe(true);
    });

    // Ground traffic only when asked, like the download of the firmware
    it('leaves out the ground traffic unless asked', () => {
        const { config } = getEspMockState();
        expect(buildSimulatedAircraft({ config }).some(aircraft => aircraft.onGround)).toBe(false);
        expect(buildSimulatedAircraft({ config: { ...config, showGroundTraffic: true } }).some(aircraft => aircraft.onGround)).toBe(true);
    });

    // The aircraft move between two downloads
    it('moves the aircraft over time', () => {
        const { config } = getEspMockState();
        const before = buildSimulatedAircraft({ config, now: 1_800_000_000_000 });
        const after = buildSimulatedAircraft({ config, now: 1_800_000_010_000 });
        const flight = before.find(aircraft => aircraft.hex === '4b1819');
        const later = after.find(aircraft => aircraft.hex === '4b1819');
        expect(later.latitude).not.toBe(flight.latitude);
    });

    // Without a location there is no center and no aircraft
    it('has no aircraft without a location', async () => {
        getEspMockState().config.hasLocation = false;
        const { response } = await callHandler(getAircraft);
        expect(response.data).toMatchObject({ center: null, status: 'no_location', aircraft: [] });
    });

    // A failing download: no aircraft yet, and the error of the firmware
    it('reports a failing download', async () => {
        getEspMockState().flightStatus = 'error';
        const { response } = await callHandler(getAircraft);
        expect(response.data).toMatchObject({ status: 'error', error: 'No answer from adsb.fi', aircraft: [] });
    });
});
