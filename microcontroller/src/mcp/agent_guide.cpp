#include "agent_guide.h"
#include <string.h>
#include <strings.h>

namespace {
	// Placeholder the device address replaces
	constexpr const char *HOST_PLACEHOLDER = "{host}";

	// The guide, with the address as a placeholder (kept in flash)
	constexpr const char *AGENT_GUIDE = R"(# Gizmo Sky Radar

A plane radar on the local network: an ESP32-S3 with a round display that shows the aircraft flying around a location its owner set. The flight data comes from adsb.fi, a community network of ADS-B receivers, and is refreshed every 5 seconds.

## Connect over MCP (recommended)

The radar is a Model Context Protocol server, with Streamable HTTP at:

```
http://{host}/mcp
```

With Claude Code:

```sh
claude mcp add --transport http gizmo-sky-radar http://{host}/mcp
```

Tools:

- `get_nearby_aircraft`: the aircraft around the radar right now, nearest first. Optional argument `limit` (1 to 64, default 15). Each aircraft has its callsign, registration, type, altitude (feet), ground speed (knots), track, vertical rate (feet per minute), squawk, distance (km) and bearing from the radar, and `inRange` when it is inside the range the display shows.
- `get_radar_status`: the location, the range, the display settings, and when the flight data was last downloaded (or why it failed).

## Plain HTTP

Without MCP, the same radar answers plain requests:

- `GET http://{host}/api/aircraft` returns the aircraft as JSON, nearest first, with the center of the radar and the age of the data.
- `GET http://{host}/api/config` returns the settings of the radar as JSON.

```sh
curl "http://{host}/api/aircraft"
```

## Tips

- The radar shows nothing until its owner sets a location in its web app: `get_radar_status` says whether it has one.
- Aircraft without a callsign are named by their registration or their ICAO address (`hex`).
- Altitudes are barometric, in feet; `onGround` aircraft are taxiing or parked.
- An `emergency` aircraft squawks 7500 (hijack), 7600 (radio failure) or 7700 (emergency), or reports one.
- Errors of the plain API are JSON: `{ "errorId": "...", "errorMessage": "...", "data": null }`.
- The radar only answers on its local network, and without authentication: anyone on that network can read it.
)";

	// Find the position of a media type in an Accept header, ignoring the case (-1 if it isn't there)
	int findMediaType(const char *accept, const char *mediaType) {
		const size_t acceptLength = strlen(accept);
		const size_t mediaTypeLength = strlen(mediaType);
		for (size_t position = 0; position + mediaTypeLength <= acceptLength; position++) {
			if (strncasecmp(accept + position, mediaType, mediaTypeLength) == 0) {
				return static_cast<int>(position);
			}
		}
		return -1;
	}
}

// Write the guide piece by piece, with the address the agent used
void writeAgentGuide(const char *host, AgentGuideWriter write, void *context) {
	const size_t placeholderLength = strlen(HOST_PLACEHOLDER);
	const char *remaining = AGENT_GUIDE;

	// Write the text between placeholders, and the address in their place
	const char *placeholder = strstr(remaining, HOST_PLACEHOLDER);
	while (placeholder != nullptr) {
		write(remaining, static_cast<size_t>(placeholder - remaining), context);
		write(host, strlen(host), context);
		remaining = placeholder + placeholderLength;
		placeholder = strstr(remaining, HOST_PLACEHOLDER);
	}

	// Write the end of the guide
	write(remaining, strlen(remaining), context);
}

// Check whether a client prefers Markdown to HTML from its Accept header
bool isMarkdownPreferred(const char *accept) {
	// No header: a plain request, which gets the web app
	if (accept == nullptr) {
		return false;
	}

	// Markdown must be listed, and come before HTML when both are (the quality values are not weighed)
	const int markdownPosition = findMediaType(accept, "text/markdown");
	const int htmlPosition = findMediaType(accept, "text/html");
	return markdownPosition >= 0 && (htmlPosition < 0 || markdownPosition < htmlPosition);
}