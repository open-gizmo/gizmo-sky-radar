// The MCP server of the mock radar: the same protocol, tools, errors and HTTP statuses as the firmware (mcp_protocol.cpp), with the simulated sky
import { MOCK_VERSION } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildAircraftPayload, buildFlightState } from '~~/server/utils/esp-mock-state.js';

/******************************** Protocol constants ********************************/

// Revision with the version in every request (_meta), and the earlier ones that open with initialize (newest first)
export const MODERN_PROTOCOL_VERSION = '2026-07-28';
export const LEGACY_PROTOCOL_VERSIONS = ['2025-11-25', '2025-06-18', '2025-03-26'];

// Keys of the per-request _meta of the 2026-07-28 revision
const META_PROTOCOL_VERSION = 'io.modelcontextprotocol/protocolVersion';
const META_CLIENT_CAPABILITIES = 'io.modelcontextprotocol/clientCapabilities';
const META_SERVER_INFO = 'io.modelcontextprotocol/serverInfo';

// JSON-RPC and MCP error codes
export const errorCodes = Object.freeze({
    PARSE_ERROR: -32700,
    INVALID_REQUEST: -32600,
    METHOD_NOT_FOUND: -32601,
    INVALID_PARAMS: -32602,
    HEADER_MISMATCH: -32020,
    UNSUPPORTED_PROTOCOL_VERSION: -32022
});

// How long clients may cache the tools and the server details
const CACHE_TTL_MS = 3600000; // 1 hour

// Most aircraft get_nearby_aircraft returns, and how many when the agent doesn't say, and the names of the tools
const AIRCRAFT_LIMIT_MAX = 64;
const AIRCRAFT_LIMIT_DEFAULT = 15;
const GET_NEARBY_AIRCRAFT_TOOL = 'get_nearby_aircraft';
const GET_RADAR_STATUS_TOOL = 'get_radar_status';

// What the server tells agents about itself (the same text as the firmware)
const SERVER_INSTRUCTIONS = 'A plane radar (an ESP32-S3 with a round display) on the local network, showing the aircraft flying around a location its owner set. Call get_nearby_aircraft to see what is in the sky right now, nearest first, and get_radar_status for its location, range and the state of its flight data. The data comes from the adsb.fi network of ADS-B receivers, refreshed every few seconds: call again to follow the aircraft. Altitudes are in feet, speeds in knots, distances in kilometers.';

/******************************** Helpers ********************************/

// Check whether a value is a plain object
const isObject = value => value !== null && typeof value === 'object' && !Array.isArray(value);

// Who the mock server is
const buildServerInfo = state => ({
    name: state.hostname || 'gizmo-sky-radar',
    title: state.name,
    version: MOCK_VERSION
});

// Answer with a JSON-RPC error and its HTTP status
const createError = ({ id = null, status, code, message, data }) => ({
    kind: 'json',
    status,
    response: {
        jsonrpc: '2.0',
        id,
        error: {
            code,
            message,
            ...(data ? { data } : {})
        }
    }
});

// Answer with a successful result, with the server's identity in its _meta
const createResult = ({ id, state, result }) => ({
    kind: 'json',
    status: 200,
    response: {
        jsonrpc: '2.0',
        id,
        result: {
            resultType: 'complete',
            _meta: { [META_SERVER_INFO]: buildServerInfo(state) },
            ...result
        }
    }
});

// Answer a tool call with an error the agent can read and correct
const createToolError = ({ id, state, message }) => createResult({
    id,
    state,
    result: { content: [{ type: 'text', text: message }], isError: true }
});

/******************************** Tools ********************************/

// Describe the tools, in the same order as the firmware
const buildTools = () => [{
    name: GET_NEARBY_AIRCRAFT_TOOL,
    title: 'Get nearby aircraft',
    description: 'List the aircraft flying around the radar right now, nearest first: callsign, registration, type, altitude (ft), ground speed (kt), track, vertical rate (ft/min), squawk, distance (km) and bearing from the radar, and whether they are inside its range. The data comes from adsb.fi and is a few seconds old.',
    annotations: { readOnlyHint: true },
    inputSchema: {
        type: 'object',
        additionalProperties: false,
        properties: {
            limit: {
                type: 'integer',
                minimum: 1,
                maximum: AIRCRAFT_LIMIT_MAX,
                description: 'Most aircraft to return, the nearest ones. Default 15.'
            }
        }
    }
}, {
    name: GET_RADAR_STATUS_TOOL,
    title: 'Get radar status',
    description: 'Describe the radar: its location, its range, its display settings, and the state of the flight data (when it was last downloaded, or why it failed).',
    annotations: { readOnlyHint: true },
    inputSchema: { type: 'object', additionalProperties: false }
}];

// Answer a tool with structured content, and the same JSON as text
const createStructuredResult = ({ id, state, content }) => createResult({
    id,
    state,
    result: { structuredContent: content, content: [{ type: 'text', text: JSON.stringify(content) }], isError: false }
});

// Answer tools/call: unknown tools and malformed params are protocol errors, wrong argument values are tool errors
const answerToolsCall = ({ id, params, state, isModern }) => {
    const errorStatus = isModern ? 400 : 200;
    const args = params.arguments ?? {};

    // Read the tool name and its arguments
    if (typeof params.name !== 'string' || !isObject(args)) {
        return createError({ id, status: errorStatus, code: errorCodes.INVALID_PARAMS, message: 'tools/call needs a tool name, and its arguments as an object' });
    }

    // List the nearest aircraft
    if (params.name === GET_NEARBY_AIRCRAFT_TOOL) {
        const unknownArgument = Object.keys(args).find(name => name !== 'limit');
        if (unknownArgument) {
            return createToolError({ id, state, message: `Unknown argument "${unknownArgument}": get_nearby_aircraft takes limit` });
        }
        if (args.limit != null && (!Number.isInteger(args.limit) || args.limit < 1 || args.limit > AIRCRAFT_LIMIT_MAX)) {
            return createToolError({ id, state, message: `limit must be a whole number from 1 to ${AIRCRAFT_LIMIT_MAX}` });
        }
        return createStructuredResult({ id, state, content: buildAircraftPayload({ state, limit: args.limit ?? AIRCRAFT_LIMIT_DEFAULT, includeTrails: false }) });
    }

    // Describe the radar
    if (params.name === GET_RADAR_STATUS_TOOL) {
        if (Object.keys(args).length > 0) {
            return createToolError({ id, state, message: 'get_radar_status takes no arguments' });
        }
        return createStructuredResult({ id, state, content: { ...state.config, ...buildFlightState(state), dataSource: 'adsb.fi', firmwareVersion: MOCK_VERSION } });
    }

    // Unknown tool
    return createError({ id, status: errorStatus, code: errorCodes.INVALID_PARAMS, message: `Unknown tool: ${params.name}` });
};

/******************************** Eras ********************************/

// Answer a request of the 2026-07-28 revision: the version, the headers and the capabilities are checked first
const answerModernRequest = ({ id, method, params, version, headers, state }) => {
    // Refuse other versions, naming the supported one
    if (version !== MODERN_PROTOCOL_VERSION) {
        return createError({ id, status: 400, code: errorCodes.UNSUPPORTED_PROTOCOL_VERSION, message: 'Unsupported protocol version', data: { supported: [MODERN_PROTOCOL_VERSION], requested: version } });
    }

    // The headers must repeat the version, the method and (for a tool call) the tool name
    const isToolCall = method === 'tools/call';
    const areHeadersValid = headers.protocolVersion === version && headers.method === method && (!isToolCall || (headers.name !== undefined && headers.name === params.name));
    if (!areHeadersValid) {
        return createError({ id, status: 400, code: errorCodes.HEADER_MISMATCH, message: 'The MCP-Protocol-Version, Mcp-Method and Mcp-Name headers must match the request' });
    }

    // Every request carries the client's capabilities
    if (!isObject(params._meta?.[META_CLIENT_CAPABILITIES])) {
        return createError({ id, status: 400, code: errorCodes.INVALID_PARAMS, message: 'Missing _meta io.modelcontextprotocol/clientCapabilities' });
    }

    // Answer the method
    switch (method) {
        // Versions, capabilities and instructions
        case 'server/discover':
            return createResult({ id, state, result: { supportedVersions: [MODERN_PROTOCOL_VERSION], capabilities: { tools: {} }, instructions: SERVER_INSTRUCTIONS, ttlMs: CACHE_TTL_MS, cacheScope: 'public' } });

        // The tools
        case 'tools/list':
            return createResult({ id, state, result: { tools: buildTools(), ttlMs: CACHE_TTL_MS, cacheScope: 'public' } });

        // A tool call
        case 'tools/call':
            return answerToolsCall({ id, params, state, isModern: true });

        // Anything else
        default:
            return createError({ id, status: 404, code: errorCodes.METHOD_NOT_FOUND, message: 'Method not found' });
    }
};

// Answer a request of an earlier revision (errors keep a 200 status: there, a 404 means "session expired" and makes the client start over)
const answerLegacyRequest = ({ id, method, params, headers, state }) => {
    // A 2026-07-28 request without its _meta, or an unknown version in the header
    if (headers.protocolVersion !== undefined && !LEGACY_PROTOCOL_VERSIONS.includes(headers.protocolVersion)) {
        if (headers.protocolVersion === MODERN_PROTOCOL_VERSION) {
            return createError({ id, status: 400, code: errorCodes.INVALID_PARAMS, message: 'Missing _meta io.modelcontextprotocol/protocolVersion' });
        }
        return createError({ id, status: 400, code: errorCodes.INVALID_REQUEST, message: 'Unsupported MCP-Protocol-Version' });
    }

    // Answer the method
    switch (method) {
        // The handshake: the client's version when the server speaks it, the newest earlier one otherwise
        case 'initialize': {
            const protocolVersion = LEGACY_PROTOCOL_VERSIONS.includes(params.protocolVersion) ? params.protocolVersion : LEGACY_PROTOCOL_VERSIONS[0];
            return createResult({ id, state, result: { protocolVersion, capabilities: { tools: {} }, serverInfo: buildServerInfo(state), instructions: SERVER_INSTRUCTIONS } });
        }

        // An empty answer
        case 'ping':
            return { kind: 'json', status: 200, response: { jsonrpc: '2.0', id, result: {} } };

        // The tools
        case 'tools/list':
            return createResult({ id, state, result: { tools: buildTools(), ttlMs: CACHE_TTL_MS, cacheScope: 'public' } });

        // A tool call
        case 'tools/call':
            return answerToolsCall({ id, params, state, isModern: false });

        // A 2026-07-28 method without its _meta
        case 'server/discover':
            return createError({ id, status: 400, code: errorCodes.INVALID_PARAMS, message: 'Missing _meta io.modelcontextprotocol/protocolVersion' });

        // Anything else
        default:
            return createError({ id, status: 200, code: errorCodes.METHOD_NOT_FOUND, message: 'Method not found' });
    }
};

/******************************** Messages ********************************/

// Read one JSON-RPC message, and say what to answer: { kind: 'json', status, response } or { kind: 'accepted' }
export const handleMcpMessage = ({ body, headers, state }) => {
    // Parse the body
    let request;
    try {
        request = JSON.parse(body);
    } catch {
        return createError({ status: 400, code: errorCodes.PARSE_ERROR, message: 'Parse error' });
    }

    // One JSON-RPC 2.0 message per request (no batches)
    if (!isObject(request) || request.jsonrpc !== '2.0') {
        return createError({ status: 400, code: errorCodes.INVALID_REQUEST, message: 'Expected one JSON-RPC 2.0 message' });
    }

    // A response from the client (it has no method): accepted with no body
    if (typeof request.method !== 'string') {
        if (request.result !== undefined || request.error !== undefined) {
            return { kind: 'accepted' };
        }
        return createError({ status: 400, code: errorCodes.INVALID_REQUEST, message: 'Expected a method' });
    }

    // A notification (it has no id): accepted with no body
    if (request.id === undefined || request.id === null) {
        return { kind: 'accepted' };
    }

    // The id is a string or a whole number
    if (typeof request.id !== 'string' && !Number.isInteger(request.id)) {
        return createError({ status: 400, code: errorCodes.INVALID_REQUEST, message: 'The id must be a string or a whole number' });
    }

    // Answer by era: a version in _meta means the 2026-07-28 revision
    const params = isObject(request.params) ? request.params : {};
    const version = params._meta?.[META_PROTOCOL_VERSION];
    if (typeof version === 'string') {
        return answerModernRequest({ id: request.id, method: request.method, params, version, headers, state });
    }
    return answerLegacyRequest({ id: request.id, method: request.method, params, headers, state });
};
