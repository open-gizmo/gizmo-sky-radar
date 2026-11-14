// Shared device and UI state for the webapp
export const useGlobalStore = () => useState('global', () => ({
    // Settings of the radar, as read from the device
    config: {
        hasLocation: false,
        latitude: null,
        longitude: null,
        locationName: '',
        rangeKm: 40,
        units: 'aviation',
        theme: 'phosphor',
        brightness: 80,
        rotation: 0,
        showLabels: true,
        showTrails: true,
        showGroundTraffic: false,
        showSweep: true
    },

    // Flag to track whether the settings have been read at least once
    configLoaded: false,

    // Flag set when the settings couldn't be read (the pages then offer to try again)
    configLoadFailed: false,

    // The last answer of /api/aircraft ({ center, rangeKm, status, dataAgeSeconds, areaTotal, error, aircraft }), or null before the first one
    traffic: null,

    // When the last answer arrived (Date.now()), to move the aircraft on between answers like the display does
    trafficReceivedAt: 0,

    // Flag set when the last read of the aircraft failed (the device didn't answer)
    trafficLoadFailed: false
}));
