#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

// The Model Context Protocol server of the radar (Streamable HTTP, JSON responses only, stateless), kept free of hardware headers so the native tests can use it
// It speaks the 2026-07-28 revision (every request carries its version in _meta) and the earlier ones, which open with initialize

// Most aircraft get_nearby_aircraft returns, and how many when the agent doesn't say
constexpr uint8_t MCP_AIRCRAFT_LIMIT_MAX = 64;
constexpr uint8_t MCP_AIRCRAFT_LIMIT_DEFAULT = 15;

// Who the server is, and how it reads the radar (the firmware gives the live data, the tests give fakes)
struct McpServerInfo {
	const char *name; // Machine name, for example "gizmo-sky-radar"
	const char *title; // Name for people, for example "Gizmo Sky Radar"
	const char *version; // Firmware version, for example "v1.0.0"
	void (*writeNearbyAircraft)(JsonObject result, uint8_t limit, void *context); // Write the radar center, the data age and the nearest aircraft (up to limit)
	void (*writeRadarStatus)(JsonObject result, void *context); // Write the location, the range, the settings and the state of the data
	void *context;
};

// The standard MCP headers of the HTTP request (nullptr when missing)
struct McpRequestHeaders {
	const char *protocolVersion; // MCP-Protocol-Version
	const char *method; // Mcp-Method
	const char *name; // Mcp-Name
};

// What the web server must send back
enum class McpReplyKind : uint8_t {
	Json, // The JSON document, with the HTTP status
	Accepted // 202 with no body (a notification, or a response from the client)
};

// Answer of the protocol to one HTTP message
struct McpReply {
	McpReplyKind kind;
	uint16_t httpStatus;
};

// Read one JSON-RPC message from the body of a POST, and write the answer into the response
McpReply handleMcpMessage(const char *body, size_t length, const McpRequestHeaders &headers, const McpServerInfo &info, JsonDocument &response);

#endif
