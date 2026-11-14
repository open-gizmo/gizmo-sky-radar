#include <unity.h>
#include <string.h>
#include <string>

#include "mcp/mcp_protocol.h"

namespace {
	// The radar, with fake data: the limit of the last aircraft list asked for
	McpServerInfo info;
	int lastLimit = -1;

	// Write two fake aircraft, and remember the limit
	void writeFakeAircraft(JsonObject result, uint8_t limit, void *context) {
		lastLimit = limit;
		result["center"]["latitude"] = 45.46;
		result["center"]["longitude"] = 9.19;
		JsonArray aircraft = result["aircraft"].to<JsonArray>();
		aircraft.add<JsonObject>()["callsign"] = "SWR123";
		aircraft.add<JsonObject>()["callsign"] = "EZY45";
		*static_cast<int *>(context) += 1;
	}

	// Write a fake status
	void writeFakeStatus(JsonObject result, void *context) {
		result["hasLocation"] = true;
		result["rangeKm"] = 40;
		*static_cast<int *>(context) += 1;
	}

	// Calls of the fake data
	int dataCalls = 0;

	// Headers of a 2026-07-28 request, and the last answer
	McpRequestHeaders headers;
	JsonDocument response;

	// Build the body of a 2026-07-28 request, with its _meta (the extra params are a JSON fragment, for example "\"name\":\"get_radar_status\"")
	std::string buildModernBody(const char *method, const char *extraParams = "", const char *id = "1") {
		std::string body = std::string("{\"jsonrpc\":\"2.0\",\"id\":") + id + ",\"method\":\"" + method + "\",\"params\":{";
		if (strlen(extraParams) > 0) {
			body += std::string(extraParams) + ",";
		}
		body += "\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\",\"io.modelcontextprotocol/clientCapabilities\":{}}}}";
		return body;
	}

	// Send a body to the server
	McpReply send(const std::string &body) {
		return handleMcpMessage(body.c_str(), body.size(), headers, info, response);
	}

	// Send a 2026-07-28 request with the headers that match it
	McpReply sendModern(const char *method, const char *extraParams = "", const char *toolName = nullptr) {
		headers = { "2026-07-28", method, toolName };
		return send(buildModernBody(method, extraParams));
	}

	// Send a request of an earlier revision (no _meta, and no headers like a client opening with initialize)
	McpReply sendLegacy(const char *method, const char *params = "{}") {
		headers = { nullptr, nullptr, nullptr };
		return send(std::string("{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"") + method + "\",\"params\":" + params + "}");
	}

	// Call a tool with arguments (a JSON object)
	McpReply callTool(const char *name, const char *arguments) {
		const std::string params = std::string("\"name\":\"") + name + "\",\"arguments\":" + arguments;
		return sendModern("tools/call", params.c_str(), name);
	}

	// Text of the first content item of a tool result
	const char *getResultText() {
		return response["result"]["content"][0]["text"] | "";
	}
}

// Start each test from the fake radar and an empty answer
void setUp() {
	dataCalls = 0;
	lastLimit = -1;
	info = { "gizmo-sky-radar", "Gizmo Sky Radar", "v1.0.0", writeFakeAircraft, writeFakeStatus, &dataCalls };
	headers = { nullptr, nullptr, nullptr };
	response.clear();
}

// Nothing to clean up after each test
void tearDown() {}

/******************************** Messages ********************************/

// A body that isn't JSON gets a parse error with a null id
void test_refuses_invalid_json() {
	const McpReply reply = send("{not json");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32700, response["error"]["code"].as<int>());
	std::string json;
	serializeJson(response, json);
	TEST_ASSERT_NOT_NULL(strstr(json.c_str(), "\"id\":null"));
}

// Batches are not part of the protocol anymore
void test_refuses_batches() {
	const McpReply reply = send("[{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}]");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32600, response["error"]["code"].as<int>());
}

// Notifications and responses of the client get 202 and no body
void test_accepts_notifications_and_client_responses() {
	TEST_ASSERT_EQUAL(McpReplyKind::Accepted, send("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}").kind);
	TEST_ASSERT_EQUAL(McpReplyKind::Accepted, send("{\"jsonrpc\":\"2.0\",\"id\":3,\"result\":{}}").kind);
	TEST_ASSERT_EQUAL_UINT16(202, send("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/cancelled\",\"params\":{}}").httpStatus);
}

// String ids come back as they are, and 0 is a valid id (not a missing one)
void test_keeps_the_request_id() {
	headers = { "2026-07-28", "tools/list", nullptr };
	send(buildModernBody("tools/list", "", "\"abc-1\""));
	TEST_ASSERT_EQUAL_STRING("abc-1", response["id"].as<const char *>());
	send(buildModernBody("tools/list", "", "0"));
	TEST_ASSERT_EQUAL_INT(0, response["id"].as<int>());
	TEST_ASSERT_FALSE(response["result"].isNull());
}

/******************************** 2026-07-28 ********************************/

// server/discover names the version, the tools capability and the server
void test_discovers_the_server() {
	const McpReply reply = sendModern("server/discover");
	TEST_ASSERT_EQUAL_UINT16(200, reply.httpStatus);
	JsonObjectConst result = response["result"];
	TEST_ASSERT_EQUAL_STRING("complete", result["resultType"]);
	TEST_ASSERT_EQUAL_STRING("2026-07-28", result["supportedVersions"][0]);
	TEST_ASSERT_TRUE(result["capabilities"]["tools"].is<JsonObjectConst>());
	TEST_ASSERT_EQUAL_STRING("gizmo-sky-radar", result["_meta"]["io.modelcontextprotocol/serverInfo"]["name"]);
	TEST_ASSERT_EQUAL_STRING("v1.0.0", result["_meta"]["io.modelcontextprotocol/serverInfo"]["version"]);
	TEST_ASSERT_NOT_NULL(result["instructions"].as<const char *>());
	TEST_ASSERT_EQUAL_STRING("public", result["cacheScope"]);
}

// Another version is refused with the supported one, so the client can retry with it
void test_refuses_other_versions() {
	headers = { "1900-01-01", "tools/list", nullptr };
	std::string body = buildModernBody("tools/list");
	body.replace(body.find("2026-07-28"), 10, "1900-01-01");
	const McpReply reply = send(body);
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32022, response["error"]["code"].as<int>());
	TEST_ASSERT_EQUAL_STRING("2026-07-28", response["error"]["data"]["supported"][0]);
	TEST_ASSERT_EQUAL_STRING("1900-01-01", response["error"]["data"]["requested"]);
}

// The headers must repeat the version, the method and the tool name
void test_checks_the_headers() {
	// A missing Mcp-Name on a tool call
	TEST_ASSERT_EQUAL_UINT16(400, sendModern("tools/call", "\"name\":\"get_radar_status\"", nullptr).httpStatus);
	TEST_ASSERT_EQUAL_INT(-32020, response["error"]["code"].as<int>());

	// An Mcp-Method that doesn't match the body
	headers = { "2026-07-28", "tools/call", nullptr };
	TEST_ASSERT_EQUAL_UINT16(400, send(buildModernBody("tools/list")).httpStatus);
	TEST_ASSERT_EQUAL_INT(-32020, response["error"]["code"].as<int>());

	// A missing MCP-Protocol-Version
	headers = { nullptr, "tools/list", nullptr };
	TEST_ASSERT_EQUAL_UINT16(400, send(buildModernBody("tools/list")).httpStatus);
}

// Every request carries the client's capabilities
void test_needs_the_client_capabilities() {
	headers = { "2026-07-28", "tools/list", nullptr };
	const McpReply reply = send("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\",\"params\":{\"_meta\":{\"io.modelcontextprotocol/protocolVersion\":\"2026-07-28\"}}}");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
}

// An unknown method is a 404
void test_answers_unknown_methods_with_404() {
	TEST_ASSERT_EQUAL_UINT16(404, sendModern("resources/list").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32601, response["error"]["code"].as<int>());
}

/******************************** Earlier revisions ********************************/

// initialize answers the client's version when the server speaks it
void test_initializes_with_the_client_version() {
	sendLegacy("initialize", "{\"protocolVersion\":\"2025-06-18\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1\"}}");
	TEST_ASSERT_EQUAL_STRING("2025-06-18", response["result"]["protocolVersion"]);
	TEST_ASSERT_EQUAL_STRING("Gizmo Sky Radar", response["result"]["serverInfo"]["title"]);
	TEST_ASSERT_TRUE(response["result"]["capabilities"]["tools"].is<JsonObjectConst>());
}

// initialize with a version the server doesn't speak gets the newest earlier one
void test_initializes_unknown_versions_with_the_newest() {
	sendLegacy("initialize", "{\"protocolVersion\":\"2024-01-01\",\"capabilities\":{}}");
	TEST_ASSERT_EQUAL_STRING("2025-11-25", response["result"]["protocolVersion"]);
}

// ping answers an empty result
void test_answers_ping() {
	TEST_ASSERT_EQUAL_UINT16(200, sendLegacy("ping").httpStatus);
	TEST_ASSERT_TRUE(response["result"].is<JsonObjectConst>());
}

// Errors of earlier revisions keep a 200 status (a 404 there tells the client its session expired)
void test_keeps_legacy_errors_on_200() {
	TEST_ASSERT_EQUAL_UINT16(200, sendLegacy("resources/list").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32601, response["error"]["code"].as<int>());
	TEST_ASSERT_EQUAL_UINT16(200, sendLegacy("tools/call", "{\"name\":\"zoom\"}").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
}

// A 2026-07-28 header without the _meta is a malformed modern request, not an earlier revision
void test_refuses_modern_requests_without_meta() {
	headers = { "2026-07-28", "tools/list", nullptr };
	const McpReply reply = send("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}");
	TEST_ASSERT_EQUAL_UINT16(400, reply.httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
}

/******************************** Tools ********************************/

// The tools come in the same order, with the limit of the aircraft list
void test_lists_the_tools() {
	sendModern("tools/list");
	JsonArrayConst tools = response["result"]["tools"];
	TEST_ASSERT_EQUAL_UINT(2, tools.size());
	TEST_ASSERT_EQUAL_STRING("get_nearby_aircraft", tools[0]["name"]);
	TEST_ASSERT_EQUAL_STRING("get_radar_status", tools[1]["name"]);
	TEST_ASSERT_EQUAL_INT(64, tools[0]["inputSchema"]["properties"]["limit"]["maximum"].as<int>());
	TEST_ASSERT_FALSE(tools[0]["inputSchema"]["additionalProperties"].as<bool>());
	TEST_ASSERT_TRUE(tools[1]["annotations"]["readOnlyHint"].as<bool>());
	TEST_ASSERT_EQUAL_STRING("complete", response["result"]["resultType"]);
}

// Without arguments, the list has the default number of aircraft, as structured content and as the same JSON in text
void test_lists_the_aircraft() {
	TEST_ASSERT_EQUAL_UINT16(200, callTool("get_nearby_aircraft", "{}").httpStatus);
	TEST_ASSERT_EQUAL_INT(15, lastLimit);
	TEST_ASSERT_FALSE(response["result"]["isError"].as<bool>());
	TEST_ASSERT_EQUAL_STRING("SWR123", response["result"]["structuredContent"]["aircraft"][0]["callsign"]);
	JsonDocument text;
	TEST_ASSERT_TRUE(deserializeJson(text, getResultText()) == DeserializationError::Ok);
	TEST_ASSERT_EQUAL_STRING("EZY45", text["aircraft"][1]["callsign"]);
}

// The limit is passed to the radar
void test_reads_the_limit() {
	callTool("get_nearby_aircraft", "{\"limit\":3}");
	TEST_ASSERT_EQUAL_INT(3, lastLimit);
	callTool("get_nearby_aircraft", "{\"limit\":64}");
	TEST_ASSERT_EQUAL_INT(64, lastLimit);
}

// Wrong arguments are tool errors the agent can read, and the radar isn't read
void test_refuses_wrong_arguments() {
	const char *wrongArguments[] = { "{\"limit\":0}", "{\"limit\":65}", "{\"limit\":2.5}", "{\"limit\":\"5\"}", "{\"radius\":10}" };
	for (const char *arguments : wrongArguments) {
		TEST_ASSERT_EQUAL_UINT16(200, callTool("get_nearby_aircraft", arguments).httpStatus);
		TEST_ASSERT_TRUE_MESSAGE(response["result"]["isError"].as<bool>(), arguments);
	}
	TEST_ASSERT_NOT_NULL(strstr(getResultText(), "radius"));
	TEST_ASSERT_EQUAL_INT(0, dataCalls);
}

// The status of the radar comes with the firmware version
void test_gets_the_radar_status() {
	callTool("get_radar_status", "{}");
	JsonObjectConst status = response["result"]["structuredContent"];
	TEST_ASSERT_TRUE(status["hasLocation"].as<bool>());
	TEST_ASSERT_EQUAL_INT(40, status["rangeKm"].as<int>());
	TEST_ASSERT_EQUAL_STRING("v1.0.0", status["firmwareVersion"]);
	TEST_ASSERT_EQUAL_INT(1, dataCalls);

	// It takes no arguments
	callTool("get_radar_status", "{\"verbose\":true}");
	TEST_ASSERT_TRUE(response["result"]["isError"].as<bool>());
}

// An unknown tool is a protocol error
void test_refuses_unknown_tools() {
	TEST_ASSERT_EQUAL_UINT16(400, sendModern("tools/call", "\"name\":\"zoom\"", "zoom").httpStatus);
	TEST_ASSERT_EQUAL_INT(-32602, response["error"]["code"].as<int>());
	TEST_ASSERT_EQUAL_STRING("Unknown tool: zoom", response["error"]["message"]); // Built in a buffer the answer must not point to
}

// Tools work the same through an earlier revision
void test_calls_tools_through_earlier_revisions() {
	sendLegacy("tools/call", "{\"name\":\"get_nearby_aircraft\",\"arguments\":{\"limit\":2}}");
	TEST_ASSERT_EQUAL_INT(2, lastLimit);
	TEST_ASSERT_EQUAL_STRING("SWR123", response["result"]["structuredContent"]["aircraft"][0]["callsign"]);
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_refuses_invalid_json);
	RUN_TEST(test_refuses_batches);
	RUN_TEST(test_accepts_notifications_and_client_responses);
	RUN_TEST(test_keeps_the_request_id);
	RUN_TEST(test_discovers_the_server);
	RUN_TEST(test_refuses_other_versions);
	RUN_TEST(test_checks_the_headers);
	RUN_TEST(test_needs_the_client_capabilities);
	RUN_TEST(test_answers_unknown_methods_with_404);
	RUN_TEST(test_initializes_with_the_client_version);
	RUN_TEST(test_initializes_unknown_versions_with_the_newest);
	RUN_TEST(test_answers_ping);
	RUN_TEST(test_keeps_legacy_errors_on_200);
	RUN_TEST(test_refuses_modern_requests_without_meta);
	RUN_TEST(test_lists_the_tools);
	RUN_TEST(test_lists_the_aircraft);
	RUN_TEST(test_reads_the_limit);
	RUN_TEST(test_refuses_wrong_arguments);
	RUN_TEST(test_gets_the_radar_status);
	RUN_TEST(test_refuses_unknown_tools);
	RUN_TEST(test_calls_tools_through_earlier_revisions);
	return UNITY_END();
}
