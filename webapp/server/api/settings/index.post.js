import { createSuccessResponse, handleEspMockError, readMockJsonBody } from 'gizmo-common/web/server/utils/gizmo-mock.js';
import { applySettings, getEspMockState, validateSettings } from '~~/server/utils/esp-mock-state.js';

export default defineEventHandler(async event => {
    try {
        // Read the body, and refuse it when a field is wrong (naming the field, like the firmware)
        const body = await readMockJsonBody(event);
        validateSettings(body);

        // Apply the fields present, and send the saved settings back
        const state = getEspMockState();
        state.config = applySettings(state.config, body);
        return createSuccessResponse({ ...state.config });
    } catch (error) {
        return handleEspMockError({ event, error, errorId: 'settings_save_failed', errorMessage: 'Failed to save the settings' });
    }
});
