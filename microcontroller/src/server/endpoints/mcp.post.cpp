#include "../server.h"
#include "../../mcp/mcp_protocol.h"

namespace {
	// JSON-RPC errors the web server sends before the protocol reads the message
	constexpr int PARSE_ERROR = -32700;
	constexpr int INVALID_REQUEST = -32600;

	// Send a JSON document with a status
	void sendJson(AsyncWebServerRequest *request, uint16_t statusCode, const JsonDocument &doc) {
		AsyncResponseStream *response = request->beginResponseStream("application/json");
		response->setCode(statusCode);
		response->addHeader("Cache-Control", "no-store");
		serializeJson(doc, *response);
		request->send(response);
	}

	// Send a JSON-RPC error before the message is read (its id is unknown)
	void sendJsonRpcError(AsyncWebServerRequest *request, uint16_t statusCode, int code, const char *message) {
		JsonDocument doc;
		doc["jsonrpc"] = "2.0";
		doc["id"] = nullptr;
		doc["error"]["code"] = code;
		doc["error"]["message"] = message;
		sendJson(request, statusCode, doc);
	}

	// Read a request header (nullptr if it's missing)
	const char *getHeaderValue(AsyncWebServerRequest *request, const char *name) {
		const AsyncWebHeader *header = request->getHeader(name);
		return header != nullptr ? header->value().c_str() : nullptr;
	}

	// The live data of the radar, for the tools
	void writeAircraftForAgent(JsonObject result, uint8_t limit, void *context) {
		writeNearbyAircraft(result, limit, false);
	}
	void writeStatusForAgent(JsonObject result, void *context) {
		writeRadarStatus(result);
	}

	// Answer one MCP message, once its body is complete
	void handleMcpRequest(AsyncWebServerRequest *request) {
		// Refuse other websites and other host names (DNS rebinding), and anything but the Wi-Fi setup through the hotspot
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Refuse bodies bigger than any MCP message to the radar
		if (request->contentLength() > gizmo::MAX_REQUEST_BODY_SIZE) {
			sendJsonRpcError(request, 413, INVALID_REQUEST, "Request too large");
			return;
		}

		// Every message is a JSON body
		const char *body = static_cast<const char *>(request->_tempObject);
		if (body == nullptr) {
			sendJsonRpcError(request, 400, PARSE_ERROR, "Missing JSON-RPC message");
			return;
		}

		// Read the message (the protocol copies what it keeps, so the body can be freed right after)
		const McpRequestHeaders headers = { getHeaderValue(request, "MCP-Protocol-Version"), getHeaderValue(request, "Mcp-Method"), getHeaderValue(request, "Mcp-Name") };
		const McpServerInfo info = { PROJECT_ID, PROJECT_NAME, PIO_FIRMWARE_VERSION, writeAircraftForAgent, writeStatusForAgent, nullptr };
		JsonDocument response;
		const McpReply reply = handleMcpMessage(body, request->contentLength(), headers, info, response);
		gizmo::releaseRequestBody(request);

		// A notification or a response of the client: accepted, with no body
		if (reply.kind == McpReplyKind::Accepted) {
			request->send(202);
			return;
		}

		// Any other answer
		sendJson(request, reply.httpStatus, response);
	}
}

// Serve the MCP server of the radar (Streamable HTTP, one JSON-RPC message per POST, answered with JSON)
void setupMcpRoute() {
	// POST: gather the body, then answer the message
	server.on("/mcp", HTTP_POST, handleMcpRequest, nullptr, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		gizmo::accumulateRequestBody(request, data, len, index, total); // A body that is too large is left out, and refused once the request is complete
	});

	// Any other method: the server offers no stream to GET, and has no session to DELETE
	server.on("/mcp", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(405);
		response->addHeader("Allow", "POST");
		request->send(response);
	});
}
