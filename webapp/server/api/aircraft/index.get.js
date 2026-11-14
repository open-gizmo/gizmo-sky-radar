import { setResponseHeader } from 'h3';
import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { buildAircraftPayload, getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Return the aircraft of the simulated sky, nearest first with their trails, like the firmware
        setResponseHeader(event, 'Cache-Control', 'no-store');
        return createSuccessResponse(buildAircraftPayload({ state: getEspMockState() }));
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_aircraft_read_failed', errorMessage: 'Failed to read the aircraft' });
    }
});
