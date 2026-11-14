import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { readBody } from 'h3';
import { mountSuspended, registerEndpoint } from '@nuxt/test-utils/runtime';
import RadarPage from '~/pages/radar.vue';
import SettingsPage from '~/pages/settings.vue';
import DevicePage from '~/pages/device.vue';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

// Remember the dialogs instead of showing them (the tests check what they say)
const shownDialogs = vi.hoisted(() => []);
vi.mock('@brumaombra/ui-vintage/message-dialog', () => ({
    showMessageDialog: dialog => shownDialogs.push(dialog)
}));

// Settings of a radar in Milan, as GET /api/config answers them
const radarSettings = {
    hasLocation: true,
    latitude: 45.4642,
    longitude: 9.19,
    locationName: 'Milano',
    rangeKm: 40,
    units: 'aviation',
    theme: 'phosphor',
    brightness: 80,
    rotation: 0,
    showLabels: true,
    showTrails: true,
    showGroundTraffic: false,
    showSweep: true
};

// Two aircraft, as GET /api/aircraft answers them
const traffic = {
    center: { latitude: 45.4642, longitude: 9.19 },
    rangeKm: 40,
    status: 'live',
    dataAgeSeconds: 2,
    areaTotal: 25,
    error: null,
    aircraft: [
        { hex: '4b1819', callsign: 'SWR2KT', registration: 'HB-JDH', type: 'A20N', description: 'AIRBUS A-320neo', squawk: '1000', category: 'A3', latitude: 45.52, longitude: 9.25, positionAgeSeconds: 1, onGround: false, altitudeFt: 35950, groundSpeedKt: 455, trackDegrees: 20, verticalRateFpm: 0, emergency: false, distanceKm: 7.6, bearingDegrees: 35, inRange: true, missing: false, trail: [[45.5, 9.24]] },
        { hex: '300843', callsign: null, registration: 'I-ZAPA', type: 'C150', description: 'CESSNA 150', squawk: '7000', category: 'A2', latitude: 45.9, longitude: 9.19, positionAgeSeconds: 3, onGround: false, altitudeFt: 1575, groundSpeedKt: 89, trackDegrees: 269, verticalRateFpm: null, emergency: false, distanceKm: 48.5, bearingDegrees: 0, inRange: false, missing: false, trail: [] }
    ]
};

// Details of the device, as GET /api/device answers them (the device card of the gizmo-common layer reads them)
const deviceDetails = { name: 'Gizmo Sky Radar', version: 'v1.0.0', uptimeSeconds: 93784, freeHeap: 187432, ssid: 'Home', accessPointActive: false, ip: '192.168.1.42', hostname: 'gizmo-sky-radar.local', signalQuality: 74 };

// The device API: its settings, its sky, and the settings it was sent
let settings;
let sky;
let savedBodies;

// Answer like the device
const registerDeviceEndpoints = () => {
    registerEndpoint('/api/config', () => ({ errorId: null, errorMessage: null, data: settings }));
    registerEndpoint('/api/aircraft', () => ({ errorId: null, errorMessage: null, data: sky }));
    registerEndpoint('/api/device', () => ({ errorId: null, errorMessage: null, data: deviceDetails }));
    registerEndpoint('/api/settings', {
        method: 'POST',
        handler: async event => {
            const body = await readBody(event);
            savedBodies.push(body);
            settings = { ...settings, ...body, hasLocation: true };
            return { errorId: null, errorMessage: null, data: settings };
        }
    });
};

// Start each test from an app that has not read the device yet
beforeEach(() => {
    settings = structuredClone(radarSettings);
    sky = structuredClone(traffic);
    savedBodies = [];
    shownDialogs.length = 0;
    const globalStore = useGlobalStore();
    globalStore.value.configLoaded = false;
    globalStore.value.configLoadFailed = false;
    globalStore.value.traffic = null;
    globalStore.value.trafficLoadFailed = false;
    vi.spyOn(console, 'error').mockImplementation(() => {});
    registerDeviceEndpoints();
});

// Clean up the mocks between tests
afterEach(() => {
    vi.restoreAllMocks();
});

// Radar page: the live sky, the list and the agents
describe('radar page', () => {
    // The aircraft are drawn and listed, nearest first, with the state of the data
    it('shows the aircraft around the radar', async () => {
        const wrapper = await mountSuspended(RadarPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('One aircraft within 22 nm'));
        expect(wrapper.find('svg').exists()).toBe(true);
        expect(wrapper.text()).toContain('SWR2KT');
        expect(wrapper.text()).toContain('FL360');
        expect(wrapper.text()).toContain('I-ZAPA'); // No callsign: the registration
        expect(wrapper.text()).toContain('Nearest aircraft');
        wrapper.unmount();
    });

    // A radar without a location asks for one instead of drawing an empty sky
    it('asks for a location', async () => {
        settings = { ...settings, hasLocation: false, latitude: null, longitude: null };
        const wrapper = await mountSuspended(RadarPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Where is your radar?'));
        expect(wrapper.text()).toContain('Set the location');
        wrapper.unmount();
    });

    // A failing download is explained, with the error of the device
    it('explains a problem with the flight data', async () => {
        sky = { ...sky, status: 'error', error: 'No answer from adsb.fi', aircraft: [] };
        const wrapper = await mountSuspended(RadarPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Flight data problem'));
        expect(wrapper.text()).toContain('No answer from adsb.fi');
        wrapper.unmount();
    });

    // Agents connect through the MCP server at the address the browser used, and the page says how
    it('shows how to connect an AI agent', async () => {
        const wrapper = await mountSuspended(RadarPage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('AI agents'));
        const mcpUrl = `${window.location.origin}/mcp`;
        expect(wrapper.text()).toContain(`claude mcp add --transport http gizmo-sky-radar ${mcpUrl}`);
        wrapper.unmount();
    });
});

// Settings page: the form of the radar
describe('settings page', () => {
    // The form starts from the settings of the device
    it('shows the settings of the device', async () => {
        const wrapper = await mountSuspended(SettingsPage);
        await vi.waitFor(() => expect(wrapper.find('#coordinates').exists()).toBe(true));
        expect(wrapper.find('#coordinates').element.value).toBe('45.46420, 9.19000');
        expect(wrapper.find('#location-name').element.value).toBe('Milano');
        expect(wrapper.text()).toContain('22 nm');
        expect(wrapper.text()).toContain('Phosphor');
    });

    // Pasted coordinates are sent as numbers, with every setting
    it('saves pasted coordinates', async () => {
        const wrapper = await mountSuspended(SettingsPage);
        await vi.waitFor(() => expect(wrapper.find('#coordinates').exists()).toBe(true));
        await wrapper.find('#coordinates').setValue('51.47, -0.4543');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(savedBodies.length).toBe(1));
        expect(savedBodies[0]).toMatchObject({ latitude: 51.47, longitude: -0.4543, locationName: 'Milano', rangeKm: 40, theme: 'phosphor', showSweep: true });
        await vi.waitFor(() => expect(shownDialogs.at(-1)?.title).toBe('Settings saved'));
    });

    // Coordinates that can't be read are never sent
    it('refuses coordinates it cannot read', async () => {
        const wrapper = await mountSuspended(SettingsPage);
        await vi.waitFor(() => expect(wrapper.find('#coordinates').exists()).toBe(true));
        await wrapper.find('#coordinates').setValue('Milan');
        await wrapper.find('form').trigger('submit');
        await vi.waitFor(() => expect(shownDialogs.length).toBe(1));
        expect(shownDialogs[0].type).toBe('error');
        expect(savedBodies).toEqual([]);
    });
});

// Device page: the device card of the layer, with the reset of the radar
describe('device page', () => {
    // The card of the layer reads GET /api/device by itself, and the reset joins its destructive rows
    it('shows the device card and the reset', async () => {
        const wrapper = await mountSuspended(DevicePage);
        await vi.waitFor(() => expect(wrapper.text()).toContain('Restart'));
        expect(wrapper.text()).toContain('Forget network');
        expect(wrapper.text()).toContain('Reset all settings');
    });
});
