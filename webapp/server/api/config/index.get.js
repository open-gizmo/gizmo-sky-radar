import { createSuccessResponse, handleEspMockError } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { getEspMockState } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Return the settings of the radar, like the firmware
        return createSuccessResponse({ ...getEspMockState().config });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'mock_config_read_failed', errorMessage: 'Failed to read the settings' });
    }
});
