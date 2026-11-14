import { IncomingMessage, ServerResponse } from 'node:http';
import { Socket } from 'node:net';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { createEvent } from 'h3';

// Stub the Nitro globals the routes of this app use before the handler modules load
vi.hoisted(() => {
    globalThis.defineEventHandler = handler => handler;
});

// Load the MCP route of the mock device
const { default: handleMcp } = await import('../../server/routes/mcp.js');
const { getEspMockState } = await import('../../server/utils/esp-mock-state.js');
const { configureGizmoMock, resetGizmoMockState } = await import('gizmo-common/web/server/utils/gizmo-mock.js');

// _meta of a 2026-07-28 request
const MODERN_META = {
    'io.modelcontextprotocol/protocolVersion': '2026-07-28',
    'io.modelcontextprotocol/clientCapabilities': {}
};

// Build a real h3 event for /mcp, with a body (a string is sent as it is) and headers
const buildEvent = ({ method = 'POST', body = undefined, headers = {} } = {}) => {
    const request = new IncomingMessage(new Socket());
    request.method = method;
    request.url = '/mcp';
    request.headers = Object.fromEntries(Object.entries(headers).map(([name, value]) => [name.toLowerCase(), value]));
    request.rawBody = Buffer.from(body === undefined ? '' : (typeof body === 'string' ? body : JSON.stringify(body)));
    return createEvent(request, new ServerResponse(request));
};

// Call the route, and return its status and answer
const callMcp = async options => {
    const event = buildEvent(options);
    const response = await handleMcp(event);
    return { status: event.node.res.statusCode, response };
};

// Send a 2026-07-28 request with the headers that match it
const sendModern = ({ method, params = {}, id = 1, toolName }) => callMcp({
    body: { jsonrpc: '2.0', id, method, params: { ...params, _meta: MODERN_META } },
    headers: {
        'MCP-Protocol-Version': '2026-07-28',
        'Mcp-Method': method,
        ...(toolName ? { 'Mcp-Name': toolName } : {})
    }
});

// Send a request of an earlier revision, like a client after initialize
const sendLegacy = ({ method, params = {}, id = 7 }) => callMcp({
    body: { jsonrpc: '2.0', id, method, params },
    headers: method === 'initialize' ? {} : { 'MCP-Protocol-Version': '2025-11-25' }
});

// Call a tool with arguments
const callTool = (name, args) => sendModern({ method: 'tools/call', params: { name, arguments: args }, toolName: name });

// Start every test from the demo device the dev server boots with, named like the radar
beforeEach(() => {
    resetGizmoMockState();
    configureGizmoMock({ name: 'Gizmo Sky Radar', hostname: 'gizmo-sky-radar' });
});

// HTTP: one JSON-RPC message per POST, like the firmware
describe('mock MCP transport', () => {
    // No stream to GET and no session to DELETE
    it('answers other methods with 405', async () => {
        const { status } = await callMcp({ method: 'GET' });
        expect(status).toBe(405);
    });

    // The body must fit the firmware buffer
    it('refuses bodies bigger than the firmware buffer', async () => {
        const { status, response } = await callMcp({ body: { jsonrpc: '2.0', id: 1, method: 'ping', params: { padding: 'x'.repeat(3000) } } });
        expect(status).toBe(413);
        expect(response.error.code).toBe(-32600);
    });

    // A body that isn't JSON gets a parse error with a null id
    it('refuses invalid JSON', async () => {
        const { status, response } = await callMcp({ body: '{not json' });
        expect(status).toBe(400);
        expect(response).toMatchObject({ id: null, error: { code: -32700 } });
    });

    // Notifications are accepted with no body
    it('accepts notifications', async () => {
        const { status } = await callMcp({ body: { jsonrpc: '2.0', method: 'notifications/initialized' } });
        expect(status).toBe(202);
    });
});

// Earlier revisions: the initialize handshake most clients still use
describe('mock MCP before 2026-07-28', () => {
    // initialize answers the client's version, the server and its tools capability
    it('initializes with the client version', async () => {
        const { status, response } = await sendLegacy({ method: 'initialize', params: { protocolVersion: '2025-06-18', capabilities: {}, clientInfo: { name: 'test', version: '1' } } });
        expect(status).toBe(200);
        expect(response.result.protocolVersion).toBe('2025-06-18');
        expect(response.result.serverInfo).toEqual({ name: 'gizmo-sky-radar', title: 'Gizmo Sky Radar', version: 'dev' });
        expect(response.result.capabilities).toEqual({ tools: {} });
    });

    // An unknown version gets the newest earlier one
    it('initializes unknown versions with the newest', async () => {
        const { response } = await sendLegacy({ method: 'initialize', params: { protocolVersion: '2024-01-01' } });
        expect(response.result.protocolVersion).toBe('2025-11-25');
    });

    // Errors keep a 200 status (a 404 there tells the client its session expired)
    it('keeps errors on 200', async () => {
        const { status, response } = await sendLegacy({ method: 'resources/list' });
        expect(status).toBe(200);
        expect(response.error.code).toBe(-32601);
    });
});

// The 2026-07-28 revision: the version in every request, checked against the headers
describe('mock MCP 2026-07-28', () => {
    // server/discover names the version, the capability and the server
    it('discovers the server', async () => {
        const { status, response } = await sendModern({ method: 'server/discover' });
        expect(status).toBe(200);
        expect(response.result).toMatchObject({ resultType: 'complete', supportedVersions: ['2026-07-28'], capabilities: { tools: {} }, cacheScope: 'public' });
        expect(response.result._meta['io.modelcontextprotocol/serverInfo'].name).toBe('gizmo-sky-radar');
    });

    // Another version is refused with the supported one
    it('refuses other versions', async () => {
        const { status, response } = await callMcp({
            body: { jsonrpc: '2.0', id: 1, method: 'tools/list', params: { _meta: { ...MODERN_META, 'io.modelcontextprotocol/protocolVersion': '1900-01-01' } } },
            headers: { 'MCP-Protocol-Version': '1900-01-01', 'Mcp-Method': 'tools/list' }
        });
        expect(status).toBe(400);
        expect(response.error).toMatchObject({ code: -32022, data: { supported: ['2026-07-28'], requested: '1900-01-01' } });
    });

    // A tool call without its Mcp-Name header is refused
    it('checks the headers', async () => {
        const { status, response } = await sendModern({ method: 'tools/call', params: { name: 'get_radar_status' } });
        expect(status).toBe(400);
        expect(response.error.code).toBe(-32020);
    });

    // An unknown method is a 404
    it('answers unknown methods with 404', async () => {
        const { status } = await sendModern({ method: 'resources/list' });
        expect(status).toBe(404);
    });
});

// Tools: the same as the firmware's
describe('mock MCP tools', () => {
    // The two tools, in order, with the limit of the aircraft list
    it('lists the tools', async () => {
        const { response } = await sendModern({ method: 'tools/list' });
        const [nearbyAircraft, radarStatus] = response.result.tools;
        expect([nearbyAircraft.name, radarStatus.name]).toEqual(['get_nearby_aircraft', 'get_radar_status']);
        expect(nearbyAircraft.inputSchema.properties.limit).toMatchObject({ type: 'integer', minimum: 1, maximum: 64 });
        expect(radarStatus.inputSchema).toEqual({ type: 'object', additionalProperties: false });
    });

    // Without arguments, the 15 nearest aircraft, nearest first, as structured content and as the same JSON in text, without their trails
    it('lists the nearest aircraft', async () => {
        const { response } = await callTool('get_nearby_aircraft', {});
        const { structuredContent, content, isError } = response.result;
        expect(isError).toBe(false);
        expect(structuredContent.center).toEqual({ latitude: 45.4642, longitude: 9.19 });
        expect(structuredContent.status).toBe('live');
        expect(structuredContent.aircraft).toHaveLength(15);
        expect(structuredContent.aircraft[0].trail).toBeUndefined();
        const distances = structuredContent.aircraft.map(aircraft => aircraft.distanceKm);
        expect(distances).toEqual([...distances].sort((first, second) => first - second));
        expect(JSON.parse(content[0].text)).toEqual(structuredContent);
    });

    // The limit is read
    it('reads the limit', async () => {
        const { response } = await callTool('get_nearby_aircraft', { limit: 3 });
        expect(response.result.structuredContent.aircraft).toHaveLength(3);
    });

    // Wrong arguments are tool errors the agent can read
    it.each([
        [{ limit: 0 }, 'limit must be a whole number'],
        [{ limit: 65 }, 'limit must be a whole number'],
        [{ limit: 2.5 }, 'limit must be a whole number'],
        [{ radius: 10 }, 'Unknown argument "radius"']
    ])('refuses %o as a tool error', async (args, message) => {
        const { status, response } = await callTool('get_nearby_aircraft', args);
        expect(status).toBe(200);
        expect(response.result.isError).toBe(true);
        expect(response.result.content[0].text).toContain(message);
    });

    // The status of the radar: its settings, the state of the data and the firmware
    it('gets the radar status', async () => {
        const { response } = await callTool('get_radar_status', {});
        expect(response.result.structuredContent).toMatchObject({ hasLocation: true, rangeKm: 40, status: 'live', dataSource: 'adsb.fi', firmwareVersion: 'dev' });
        const { response: refused } = await callTool('get_radar_status', { verbose: true });
        expect(refused.result.isError).toBe(true);
    });

    // A radar without a location has no aircraft, and says so
    it('says when the radar has no location', async () => {
        getEspMockState().config.hasLocation = false;
        const { response } = await callTool('get_nearby_aircraft', {});
        expect(response.result.structuredContent).toMatchObject({ center: null, status: 'no_location', aircraft: [] });
    });

    // An unknown tool is a protocol error
    it('refuses unknown tools', async () => {
        const { status, response } = await sendModern({ method: 'tools/call', params: { name: 'zoom' }, toolName: 'zoom' });
        expect(status).toBe(400);
        expect(response.error).toMatchObject({ code: -32602, message: 'Unknown tool: zoom' });
    });
});
