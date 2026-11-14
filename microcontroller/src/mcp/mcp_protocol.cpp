#include "mcp_protocol.h"
#include <stdio.h>
#include <string.h>
#include <string>

namespace {
	/******************************** Protocol constants ********************************/

	// Revision with the version in every request (_meta), and the earlier ones that open with initialize (newest first)
	constexpr const char *MODERN_PROTOCOL_VERSION = "2026-07-28";
	constexpr const char *LEGACY_PROTOCOL_VERSIONS[] = { "2025-11-25", "2025-06-18", "2025-03-26" };

	// Keys of the per-request _meta of the 2026-07-28 revision
	constexpr const char *META_PROTOCOL_VERSION = "io.modelcontextprotocol/protocolVersion";
	constexpr const char *META_CLIENT_CAPABILITIES = "io.modelcontextprotocol/clientCapabilities";
	constexpr const char *META_SERVER_INFO = "io.modelcontextprotocol/serverInfo";

	// JSON-RPC and MCP error codes
	constexpr int PARSE_ERROR = -32700;
	constexpr int INVALID_REQUEST = -32600;
	constexpr int METHOD_NOT_FOUND = -32601;
	constexpr int INVALID_PARAMS = -32602;
	constexpr int HEADER_MISMATCH = -32020;
	constexpr int UNSUPPORTED_PROTOCOL_VERSION = -32022;

	// How long clients may cache the tools and the server details (the firmware only changes them with an update)
	constexpr uint32_t CACHE_TTL_MS = 3600000; // 1 hour

	// Names of the tools
	constexpr const char *GET_NEARBY_AIRCRAFT_TOOL = "get_nearby_aircraft";
	constexpr const char *GET_RADAR_STATUS_TOOL = "get_radar_status";

	// What the server tells agents about itself
	constexpr const char *SERVER_INSTRUCTIONS = "A plane radar (an ESP32-S3 with a round display) on the local network, showing the aircraft flying around a location its owner set. Call get_nearby_aircraft to see what is in the sky right now, nearest first, and get_radar_status for its location, range and the state of its flight data. The data comes from the adsb.fi network of ADS-B receivers, refreshed every few seconds: call again to follow the aircraft. Altitudes are in feet, speeds in knots, distances in kilometers.";

	// Era of a request: the 2026-07-28 revision, or an earlier one
	enum class ProtocolEra : uint8_t {
		Modern,
		Legacy
	};

	/******************************** Helpers ********************************/

	// Copy a text into the document (ArduinoJson only keeps a pointer to a const char *, which would dangle once a buffer or the request is gone)
	JsonString copyText(const char *text) {
		return JsonString(text, JsonString::Copied);
	}

	// Check whether a version is one of the earlier revisions the server speaks
	bool isLegacyProtocolVersion(const char *version) {
		for (const char *legacyVersion : LEGACY_PROTOCOL_VERSIONS) {
			if (strcmp(version, legacyVersion) == 0) {
				return true;
			}
		}
		return false;
	}

	// Compare a header with the value it must repeat (a missing header never matches)
	bool isHeaderEqual(const char *header, const char *value) {
		return header != nullptr && value != nullptr && strcmp(header, value) == 0;
	}

	// Start a JSON-RPC answer with the id of the request
	JsonObject startAnswer(JsonDocument &response, JsonVariantConst id) {
		response.clear();
		response["jsonrpc"] = "2.0";
		if (id.isNull()) {
			response["id"] = nullptr; // A request whose id couldn't be read is answered with a null id
		} else {
			response["id"] = id;
		}
		return response.as<JsonObject>();
	}

	// Write a JSON-RPC error, returning the reply with its HTTP status
	McpReply writeError(JsonDocument &response, JsonVariantConst id, uint16_t httpStatus, int code, const char *message) {
		JsonObject answer = startAnswer(response, id);
		JsonObject error = answer["error"].to<JsonObject>();
		error["code"] = code;
		error["message"] = copyText(message);
		return { McpReplyKind::Json, httpStatus };
	}

	// Start a successful result, with the server's identity in its _meta
	JsonObject startResult(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info) {
		JsonObject result = startAnswer(response, id)["result"].to<JsonObject>();
		result["resultType"] = "complete";
		JsonObject serverInfo = result["_meta"][META_SERVER_INFO].to<JsonObject>();
		serverInfo["name"] = info.name;
		serverInfo["title"] = info.title;
		serverInfo["version"] = info.version;
		return result;
	}

	// Reply with the JSON document and a 200 status
	McpReply replyJson() {
		return { McpReplyKind::Json, 200 };
	}

	// Write a tool result with one text item, as an error the agent can read and correct
	McpReply writeToolError(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info, const char *message) {
		JsonObject result = startResult(response, id, info);
		JsonObject text = result["content"].add<JsonObject>();
		text["type"] = "text";
		text["text"] = copyText(message);
		result["isError"] = true;
		return replyJson();
	}

	/******************************** Server details ********************************/

	// Write the capabilities of the server: tools only, whose list never changes while it runs
	void writeCapabilities(JsonObject result) {
		result["capabilities"]["tools"].to<JsonObject>();
	}

	// Answer server/discover (2026-07-28): versions, capabilities and instructions
	McpReply answerDiscover(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info) {
		JsonObject result = startResult(response, id, info);
		result["supportedVersions"].add(MODERN_PROTOCOL_VERSION);
		writeCapabilities(result);
		result["instructions"] = SERVER_INSTRUCTIONS;
		result["ttlMs"] = CACHE_TTL_MS;
		result["cacheScope"] = "public";
		return replyJson();
	}

	// Answer initialize (earlier revisions): the client's version when the server speaks it, the newest earlier one otherwise
	McpReply answerInitialize(JsonDocument &response, JsonVariantConst id, JsonVariantConst params, const McpServerInfo &info) {
		const char *requestedVersion = params["protocolVersion"] | "";
		JsonObject result = startResult(response, id, info);
		result["protocolVersion"] = copyText(isLegacyProtocolVersion(requestedVersion) ? requestedVersion : LEGACY_PROTOCOL_VERSIONS[0]);
		writeCapabilities(result);
		JsonObject serverInfo = result["serverInfo"].to<JsonObject>();
		serverInfo["name"] = info.name;
		serverInfo["title"] = info.title;
		serverInfo["version"] = info.version;
		result["instructions"] = SERVER_INSTRUCTIONS;
		return replyJson();
	}

	/******************************** Tools ********************************/

	// Write the description of get_nearby_aircraft
	void writeGetNearbyAircraftTool(JsonArray tools) {
		JsonObject tool = tools.add<JsonObject>();
		tool["name"] = GET_NEARBY_AIRCRAFT_TOOL;
		tool["title"] = "Get nearby aircraft";
		tool["description"] = "List the aircraft flying around the radar right now, nearest first: callsign, registration, type, altitude (ft), ground speed (kt), track, vertical rate (ft/min), squawk, distance (km) and bearing from the radar, and whether they are inside its range. The data comes from adsb.fi and is a few seconds old.";
		tool["annotations"]["readOnlyHint"] = true;

		// Input: how many aircraft, optional
		JsonObject schema = tool["inputSchema"].to<JsonObject>();
		schema["type"] = "object";
		schema["additionalProperties"] = false;
		JsonObject limit = schema["properties"]["limit"].to<JsonObject>();
		limit["type"] = "integer";
		limit["minimum"] = 1;
		limit["maximum"] = MCP_AIRCRAFT_LIMIT_MAX;
		limit["description"] = "Most aircraft to return, the nearest ones. Default 15.";
	}

	// Write the description of get_radar_status
	void writeGetRadarStatusTool(JsonArray tools) {
		JsonObject tool = tools.add<JsonObject>();
		tool["name"] = GET_RADAR_STATUS_TOOL;
		tool["title"] = "Get radar status";
		tool["description"] = "Describe the radar: its location, its range, its display settings, and the state of the flight data (when it was last downloaded, or why it failed).";
		tool["annotations"]["readOnlyHint"] = true;
		JsonObject schema = tool["inputSchema"].to<JsonObject>();
		schema["type"] = "object";
		schema["additionalProperties"] = false;
	}

	// Answer tools/list (always in the same order)
	McpReply answerToolsList(JsonDocument &response, JsonVariantConst id, const McpServerInfo &info) {
		JsonObject result = startResult(response, id, info);
		JsonArray tools = result["tools"].to<JsonArray>();
		writeGetNearbyAircraftTool(tools);
		writeGetRadarStatusTool(tools);
		result["ttlMs"] = CACHE_TTL_MS;
		result["cacheScope"] = "public";
		return replyJson();
	}

	// Find an argument the tool doesn't take (nullptr if there is none)
	const char *findUnknownArgument(JsonObjectConst arguments, const char *const *knownNames, size_t knownCount) {
		for (JsonPairConst argument : arguments) {
			bool isKnown = false;
			for (size_t index = 0; index < knownCount; index++) {
				isKnown = isKnown || strcmp(argument.key().c_str(), knownNames[index]) == 0;
			}
			if (!isKnown) {
				return argument.key().c_str();
			}
		}
		return nullptr;
	}

	// End a structured result: the same JSON as text, for clients that don't read structured content
	McpReply finishStructuredResult(JsonObject result) {
		std::string json;
		serializeJson(result["structuredContent"], json);
		JsonObject text = result["content"].add<JsonObject>();
		text["type"] = "text";
		text["text"] = json;
		result["isError"] = false;
		return replyJson();
	}

	// Call get_nearby_aircraft: the nearest aircraft as structured content
	McpReply callGetNearbyAircraft(JsonDocument &response, JsonVariantConst id, JsonObjectConst arguments, const McpServerInfo &info) {
		// Refuse arguments the tool doesn't take
		const char *const knownNames[] = { "limit" };
		const char *unknownArgument = findUnknownArgument(arguments, knownNames, 1);
		if (unknownArgument != nullptr) {
			char message[96];
			snprintf(message, sizeof(message), "Unknown argument \"%s\": get_nearby_aircraft takes limit", unknownArgument);
			return writeToolError(response, id, info, message);
		}

		// Read the limit (a whole number in range: 10.5 or "10" are refused)
		uint8_t limit = MCP_AIRCRAFT_LIMIT_DEFAULT;
		JsonVariantConst limitValue = arguments["limit"];
		if (!limitValue.isNull()) {
			if (!limitValue.is<int>() || limitValue.as<int>() < 1 || limitValue.as<int>() > MCP_AIRCRAFT_LIMIT_MAX) {
				char message[64];
				snprintf(message, sizeof(message), "limit must be a whole number from 1 to %u", static_cast<unsigned>(MCP_AIRCRAFT_LIMIT_MAX));
				return writeToolError(response, id, info, message);
			}
			limit = static_cast<uint8_t>(limitValue.as<int>());
		}

		// List the aircraft
		JsonObject result = startResult(response, id, info);
		info.writeNearbyAircraft(result["structuredContent"].to<JsonObject>(), limit, info.context);
		return finishStructuredResult(result);
	}

	// Call get_radar_status: the radar as structured content
	McpReply callGetRadarStatus(JsonDocument &response, JsonVariantConst id, JsonObjectConst arguments, const McpServerInfo &info) {
		// The tool takes no arguments
		if (findUnknownArgument(arguments, nullptr, 0) != nullptr) {
			return writeToolError(response, id, info, "get_radar_status takes no arguments");
		}

		// Describe the radar
		JsonObject result = startResult(response, id, info);
		JsonObject status = result["structuredContent"].to<JsonObject>();
		info.writeRadarStatus(status, info.context);
		status["firmwareVersion"] = info.version;
		return finishStructuredResult(result);
	}

	// Answer tools/call: unknown tools and malformed params are protocol errors, wrong argument values are tool errors
	McpReply answerToolsCall(JsonDocument &response, JsonVariantConst id, JsonVariantConst params, const McpServerInfo &info, ProtocolEra era) {
		const uint16_t errorStatus = era == ProtocolEra::Modern ? 400 : 200;

		// Read the tool name and its arguments (missing arguments are none)
		JsonVariantConst name = params["name"];
		JsonVariantConst arguments = params["arguments"];
		if (!name.is<const char *>() || !(arguments.isNull() || arguments.is<JsonObjectConst>())) {
			return writeError(response, id, errorStatus, INVALID_PARAMS, "tools/call needs a tool name, and its arguments as an object");
		}

		// Call the tool
		const char *toolName = name.as<const char *>();
		if (strcmp(toolName, GET_NEARBY_AIRCRAFT_TOOL) == 0) {
			return callGetNearbyAircraft(response, id, arguments.as<JsonObjectConst>(), info);
		}
		if (strcmp(toolName, GET_RADAR_STATUS_TOOL) == 0) {
			return callGetRadarStatus(response, id, arguments.as<JsonObjectConst>(), info);
		}

		// Unknown tool
		char message[96];
		snprintf(message, sizeof(message), "Unknown tool: %s", toolName);
		return writeError(response, id, errorStatus, INVALID_PARAMS, message);
	}

	/******************************** Eras ********************************/

	// Answer a request of the 2026-07-28 revision: the version, the headers and the capabilities are checked first
	McpReply answerModernRequest(JsonDocument &response, JsonVariantConst id, const char *method, JsonVariantConst params, const char *version, const McpRequestHeaders &headers, const McpServerInfo &info) {
		// Refuse other versions, naming the supported one
		if (strcmp(version, MODERN_PROTOCOL_VERSION) != 0) {
			McpReply reply = writeError(response, id, 400, UNSUPPORTED_PROTOCOL_VERSION, "Unsupported protocol version");
			JsonObject data = response["error"]["data"].to<JsonObject>();
			data["supported"].add(MODERN_PROTOCOL_VERSION);
			data["requested"] = copyText(version);
			return reply;
		}

		// The headers must repeat the version, the method and (for a tool call) the tool name
		const bool isToolCall = strcmp(method, "tools/call") == 0;
		const bool areHeadersValid = isHeaderEqual(headers.protocolVersion, version) && isHeaderEqual(headers.method, method) && (!isToolCall || isHeaderEqual(headers.name, params["name"].as<const char *>()));
		if (!areHeadersValid) {
			return writeError(response, id, 400, HEADER_MISMATCH, "The MCP-Protocol-Version, Mcp-Method and Mcp-Name headers must match the request");
		}

		// Every request carries the client's capabilities
		if (!params["_meta"][META_CLIENT_CAPABILITIES].is<JsonObjectConst>()) {
			return writeError(response, id, 400, INVALID_PARAMS, "Missing _meta io.modelcontextprotocol/clientCapabilities");
		}

		// Answer the method
		if (strcmp(method, "server/discover") == 0) {
			return answerDiscover(response, id, info);
		}
		if (strcmp(method, "tools/list") == 0) {
			return answerToolsList(response, id, info);
		}
		if (isToolCall) {
			return answerToolsCall(response, id, params, info, ProtocolEra::Modern);
		}
		return writeError(response, id, 404, METHOD_NOT_FOUND, "Method not found");
	}

	// Answer a request of an earlier revision (errors keep a 200 status: there, a 404 means "session expired" and makes the client start over)
	McpReply answerLegacyRequest(JsonDocument &response, JsonVariantConst id, const char *method, JsonVariantConst params, const McpRequestHeaders &headers, const McpServerInfo &info) {
		// A 2026-07-28 request without its _meta, or an unknown version in the header
		if (headers.protocolVersion != nullptr && !isLegacyProtocolVersion(headers.protocolVersion)) {
			if (strcmp(headers.protocolVersion, MODERN_PROTOCOL_VERSION) == 0) {
				return writeError(response, id, 400, INVALID_PARAMS, "Missing _meta io.modelcontextprotocol/protocolVersion");
			}
			return writeError(response, id, 400, INVALID_REQUEST, "Unsupported MCP-Protocol-Version");
		}

		// Answer the method
		if (strcmp(method, "initialize") == 0) {
			return answerInitialize(response, id, params, info);
		}
		if (strcmp(method, "ping") == 0) {
			startAnswer(response, id)["result"].to<JsonObject>();
			return replyJson();
		}
		if (strcmp(method, "tools/list") == 0) {
			return answerToolsList(response, id, info);
		}
		if (strcmp(method, "tools/call") == 0) {
			return answerToolsCall(response, id, params, info, ProtocolEra::Legacy);
		}
		if (strcmp(method, "server/discover") == 0) {
			return writeError(response, id, 400, INVALID_PARAMS, "Missing _meta io.modelcontextprotocol/protocolVersion");
		}
		return writeError(response, id, 200, METHOD_NOT_FOUND, "Method not found");
	}
}

// Read one JSON-RPC message from the body of a POST, and write the answer into the response
McpReply handleMcpMessage(const char *body, size_t length, const McpRequestHeaders &headers, const McpServerInfo &info, JsonDocument &response) {
	// Parse the body (a const body is copied, so it can be freed right after)
	JsonDocument request;
	if (deserializeJson(request, body, length) != DeserializationError::Ok) {
		return writeError(response, JsonVariantConst(), 400, PARSE_ERROR, "Parse error");
	}

	// One JSON-RPC 2.0 message per request (no batches)
	JsonVariantConst id = request["id"];
	if (!request.is<JsonObjectConst>() || strcmp(request["jsonrpc"] | "", "2.0") != 0) {
		return writeError(response, JsonVariantConst(), 400, INVALID_REQUEST, "Expected one JSON-RPC 2.0 message");
	}

	// A response from the client (it has no method): accepted with no body
	if (!request["method"].is<const char *>()) {
		if (!request["result"].isNull() || !request["error"].isNull()) {
			return { McpReplyKind::Accepted, 202 };
		}
		return writeError(response, JsonVariantConst(), 400, INVALID_REQUEST, "Expected a method");
	}

	// A notification (it has no id): accepted with no body
	if (id.isNull()) {
		return { McpReplyKind::Accepted, 202 };
	}

	// The id is a string or a whole number
	if (!id.is<const char *>() && !id.is<long long>()) {
		return writeError(response, JsonVariantConst(), 400, INVALID_REQUEST, "The id must be a string or a whole number");
	}

	// Answer by era: a version in _meta means the 2026-07-28 revision
	const char *method = request["method"].as<const char *>();
	JsonVariantConst params = request["params"];
	const char *version = params["_meta"][META_PROTOCOL_VERSION];
	if (version != nullptr) {
		return answerModernRequest(response, id, method, params, version, headers, info);
	}
	return answerLegacyRequest(response, id, method, params, headers, info);
}
