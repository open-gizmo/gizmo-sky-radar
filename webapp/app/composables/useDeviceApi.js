// The request wrapper (fetchEsp) and the Wi-Fi and device calls come from the gizmo-common layer: only the calls of the radar are here

// Fetch the settings of the radar
export const getConfig = async () => {
    const response = await fetchEsp('/api/config');
    return response.data || {};
};

// Save settings of the radar (only the fields sent change), and return the settings the device saved
export const saveSettings = async settings => {
    const response = await fetchEsp('/api/settings', {
        method: 'POST',
        body: settings
    });
    return response.data || {};
};

// Forget the settings and the saved network, then restart the device into its setup hotspot
export const resetSettings = async () => {
    // POST, so other pages can't trigger it with a simple link
    await fetchEsp('/api/reset-settings', { method: 'POST' });
};

// Fetch the aircraft around the radar, nearest first, with the state of the flight data
export const getAircraft = async () => {
    const response = await fetchEsp('/api/aircraft', { retry: 0 });
    return response.data || {};
};
