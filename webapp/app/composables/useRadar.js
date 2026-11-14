import { getAircraft, getConfig, saveSettings } from '~/composables/useDeviceApi.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

// Load the settings of the radar into the store, and remember a failure (so the pages never save the store defaults)
export const loadRadarConfig = async () => {
    const globalStore = useGlobalStore();

    try {
        // Get the settings
        const config = await getConfig();

        // Merge them with the store and mark them as loaded
        globalStore.value.config = {
            ...globalStore.value.config,
            ...config
        };
        globalStore.value.configLoaded = true;
        globalStore.value.configLoadFailed = false;
    } catch (error) {
        // Remember the failure, so the pages offer to try again
        globalStore.value.configLoadFailed = true;
        throw error;
    }
};

// Save settings, and keep what the device saved
export const saveRadarSettings = async settings => {
    const globalStore = useGlobalStore();
    const saved = await saveSettings(settings);
    globalStore.value.config = {
        ...globalStore.value.config,
        ...saved
    };
};

// Read the aircraft around the radar into the store (a failure is remembered, the last aircraft stay)
export const loadTraffic = async () => {
    const globalStore = useGlobalStore();

    try {
        globalStore.value.traffic = await getAircraft();
        globalStore.value.trafficReceivedAt = Date.now();
        globalStore.value.trafficLoadFailed = false;
    } catch (error) {
        globalStore.value.trafficLoadFailed = true;
        throw error;
    }
};
