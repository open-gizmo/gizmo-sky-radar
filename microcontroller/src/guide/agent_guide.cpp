#include "agent_guide.h"
#include <string.h>
#include <strings.h>

namespace {
	// Placeholder the device address replaces
	constexpr const char *HOST_PLACEHOLDER = "{host}";

	// The guide, with the address as a placeholder (kept in flash)
	constexpr const char *AGENT_GUIDE = R"(# Gizmo Sky Radar

A plane radar on the local network: an ESP32-S3 with a round display that shows the aircraft flying around a location its owner set. The flight data comes from adsb.fi, a community network of ADS-B receivers, and is refreshed every 5 seconds.

## HTTP API

The radar answers plain HTTP requests on the local network:

- `GET http://{host}/api/aircraft` returns the aircraft as JSON, nearest first, with the center of the radar and the age of the data.
- `GET http://{host}/api/config` returns the settings of the radar as JSON.

```sh
curl "http://{host}/api/aircraft"
```

## Tips

- The radar shows nothing until its owner sets a location in its web app: `hasLocation` in `/api/config` says whether it has one, and the `status` of `/api/aircraft` is then `no_location`.
- Aircraft without a callsign are named by their registration or their ICAO address (`hex`).
- Altitudes are barometric, in feet, speeds in knots, distances in kilometers; `onGround` aircraft are taxiing or parked.
- The data is a few seconds old: move an aircraft on along `trackDegrees` at `groundSpeedKt` for `positionAgeSeconds` to know where it is now. Read it again to follow the aircraft.
- An `emergency` aircraft squawks 7500 (hijack), 7600 (radio failure) or 7700 (emergency), or reports one.
- Errors of the API are JSON: `{ "errorId": "...", "errorMessage": "...", "data": null }`.
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