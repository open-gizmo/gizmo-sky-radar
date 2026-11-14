import { getHeader, getMethod, readRawBody, setResponseHeader, setResponseStatus } from 'h3';
import { MAX_REQUEST_BODY_SIZE, byteLength } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { getEspMockState } from '~~/server/utils/esp-mock-state.js';
import { errorCodes, handleMcpMessage } from '~~/server/utils/mcp-mock.js';

// Send a JSON answer with its status
const sendJson = (event, status, response) => {
    setResponseStatus(event, status);
    setResponseHeader(event, 'Content-Type', 'application/json');
    setResponseHeader(event, 'Cache-Control', 'no-store');
    return response;
};

// The MCP server of the mock radar (Streamable HTTP, one JSON-RPC message per POST, answered with JSON), like /mcp on the firmware
export default defineEventHandler(async event => {
    // Any other method: the server offers no stream to GET, and has no session to DELETE
    if (getMethod(event) !== 'POST') {
        setResponseStatus(event, 405);
        setResponseHeader(event, 'Allow', 'POST');
        return '';
    }

    // Read the body, which must fit the firmware buffer
    const body = (await readRawBody(event, 'utf8')) || '';
    if (byteLength(body) > MAX_REQUEST_BODY_SIZE) {
        return sendJson(event, 413, { jsonrpc: '2.0', id: null, error: { code: errorCodes.INVALID_REQUEST, message: 'Request too large' } });
    }
    if (body === '') {
        return sendJson(event, 400, { jsonrpc: '2.0', id: null, error: { code: errorCodes.PARSE_ERROR, message: 'Missing JSON-RPC message' } });
    }

    // Read the message with the standard MCP headers (undefined when missing)
    const headers = {
        protocolVersion: getHeader(event, 'mcp-protocol-version'),
        method: getHeader(event, 'mcp-method'),
        name: getHeader(event, 'mcp-name')
    };
    const reply = handleMcpMessage({ body, headers, state: getEspMockState() });

    // A notification or a response of the client: accepted, with no body
    if (reply.kind === 'accepted') {
        setResponseStatus(event, 202);
        return '';
    }

    // Any other answer
    return sendJson(event, reply.status, reply.response);
});
