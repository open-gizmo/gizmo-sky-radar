<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import Airplane01Icon from '@hugeicons/core-free-icons/Airplane01Icon';
import AirplaneTakeOff01Icon from '@hugeicons/core-free-icons/AirplaneTakeOff01Icon';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import BookOpen01Icon from '@hugeicons/core-free-icons/BookOpen01Icon';
import Compass01Icon from '@hugeicons/core-free-icons/Compass01Icon';
import DashboardSpeed01Icon from '@hugeicons/core-free-icons/DashboardSpeed01Icon';
import HashIcon from '@hugeicons/core-free-icons/HashIcon';
import LinkSquare02Icon from '@hugeicons/core-free-icons/LinkSquare02Icon';
import Location01Icon from '@hugeicons/core-free-icons/Location01Icon';
import MountainIcon from '@hugeicons/core-free-icons/MountainIcon';
import Navigation03Icon from '@hugeicons/core-free-icons/Navigation03Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import RoboticIcon from '@hugeicons/core-free-icons/RoboticIcon';
import Tag01Icon from '@hugeicons/core-free-icons/Tag01Icon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { Alert, AlertDescription, AlertTitle } from '@brumaombra/ui-vintage/alert';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { Card } from '@brumaombra/ui-vintage/card';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { ResponsiveDialog } from '@brumaombra/ui-vintage/responsive-dialog';
import { loadRadarConfig, loadTraffic } from '~/composables/useRadar.js';
import { ALTITUDE_BANDS, GROUND_COLOR, buildTrackerUrl, formatAltitude, formatDistance, formatRange, formatSpeed, formatVerticalRate, getAircraftName, getAltitudeColor, getCompassPoint } from '~/composables/useUtils.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const POLL_INTERVAL_MS = 5000; // The device downloads new data every 5 seconds
const LIST_LIMIT = 20; // Aircraft in the list below the radar
const globalStore = useGlobalStore();
const { t, locale } = useI18n();
const localePath = useLocalePath();
const selectedHex = ref('');
const isDetailsOpen = ref(false);

// Settings of the radar and the last aircraft read
const config = computed(() => globalStore.value.config);
const traffic = computed(() => globalStore.value.traffic);
const aircraft = computed(() => traffic.value?.aircraft || []);

// Format the numbers of an aircraft in the units of the radar
const units = computed(() => config.value.units);
const describeAltitude = item => formatAltitude({ altitudeFt: item.altitudeFt, onGround: item.onGround, units: units.value, locale: locale.value });
const describeDistance = item => `${formatDistance({ distanceKm: item.distanceKm, units: units.value, locale: locale.value })} ${getCompassPoint(item.bearingDegrees)}`;

// The aircraft inside the range
const inRangeCount = computed(() => aircraft.value.filter(item => item.inRange).length);

// The aircraft of the list: the nearest ones, with their name, what they are and where
const listedAircraft = computed(() => aircraft.value.slice(0, LIST_LIMIT).map(item => ({
    hex: item.hex,
    name: getAircraftName(item),
    description: `${item.type || t('pages.radar.unknownType')} · ${describeDistance(item)}`,
    value: describeAltitude(item),
    color: item.emergency ? '#ff2d2d' : getAltitudeColor(item),
    outOfRange: !item.inRange
})));

// The aircraft picked on the radar or in the list
const selectedAircraft = computed(() => aircraft.value.find(item => item.hex === selectedHex.value) || null);

// The rows of its details
const selectedDetails = computed(() => {
    const item = selectedAircraft.value;
    if (!item) return [];
    return [
        { label: t('pages.radar.details.type'), icon: Airplane01Icon, value: [item.type, item.description].filter(Boolean).join(' · ') || t('pages.radar.unknownType') },
        { label: t('pages.radar.details.registration'), icon: Tag01Icon, value: item.registration || '—' },
        { label: t('pages.radar.details.altitude'), icon: MountainIcon, value: describeAltitude(item) },
        { label: t('pages.radar.details.speed'), icon: DashboardSpeed01Icon, value: formatSpeed({ speedKt: item.groundSpeedKt, units: units.value, locale: locale.value }) },
        { label: t('pages.radar.details.verticalRate'), icon: AirplaneTakeOff01Icon, value: formatVerticalRate({ verticalRateFpm: item.verticalRateFpm, units: units.value, locale: locale.value }) },
        { label: t('pages.radar.details.track'), icon: Compass01Icon, value: item.trackDegrees == null ? '—' : `${Math.round(item.trackDegrees)}° ${getCompassPoint(item.trackDegrees)}` },
        { label: t('pages.radar.details.distance'), icon: Navigation03Icon, value: describeDistance(item) },
        { label: t('pages.radar.details.squawk'), icon: HashIcon, value: item.squawk || '—' }
    ];
});

// The state of the flight data, as a sentence under the radar
const statusText = computed(() => {
    const data = traffic.value;
    if (!data) return '';
    const range = formatRange({ rangeKm: config.value.rangeKm, units: units.value, locale: locale.value });
    switch (data.status) {
        case 'live':
            return inRangeCount.value === 0 ? t('pages.radar.status.clear', { range }) : t('pages.radar.status.live', { count: inRangeCount.value, range }, inRangeCount.value);
        case 'stale':
            return t('pages.radar.status.stale', { minutes: Math.floor((data.dataAgeSeconds || 0) / 60) });
        case 'error':
            return t('pages.radar.status.error');
        case 'waiting_for_network':
            return t('pages.radar.status.waitingForNetwork');
        default:
            return t('pages.radar.status.loading');
    }
});

// Whether the data has a problem worth a warning
const hasDataProblem = computed(() => ['error', 'stale'].includes(traffic.value?.status));

// Bands of the legend: the ground, then each altitude color from where it starts (the lowest up to where it ends)
const legend = computed(() => {
    const formatLimit = altitudeFt => formatAltitude({ altitudeFt, onGround: false, units: units.value === 'aviation' ? 'imperial' : units.value, locale: locale.value });
    const bands = ALTITUDE_BANDS.map((band, index) => ({
        color: band.color,
        label: index === 0 ? `< ${formatLimit(band.maxFt)}` : `${formatLimit(ALTITUDE_BANDS[index - 1].maxFt)}+`
    }));
    return [{ color: GROUND_COLOR, label: t('pages.radar.legendGround') }, ...bands];
});

// Address of the MCP server, as this browser reached the device (an agent on the same network uses the same one)
const mcpUrl = computed(() => `${window.location.origin}/mcp`);

// Help under the agents group: the command that adds the radar to Claude Code
const agentsHint = computed(() => t('pages.radar.agents.hint', { command: `claude mcp add --transport http gizmo-sky-radar ${mcpUrl.value}` }));

// Pick an aircraft (from the radar: its details show under it; from the list: they open in a dialog)
const handleRadarSelect = hex => {
    selectedHex.value = hex;
};
const handleListSelect = hex => {
    selectedHex.value = hex;
    isDetailsOpen.value = true;
};

// Open the aircraft on the adsb.fi map
const handleTrackerPress = () => {
    window.open(buildTrackerUrl(selectedHex.value), '_blank', 'noopener');
};

// Open the guide for AI agents in a new tab
const handleAgentGuidePress = () => {
    window.open('/llms.txt', '_blank', 'noopener');
};

// Read the aircraft, then again every few seconds while the page is open (a failure keeps the last ones on screen)
let pollTimer = null;
let isPolling = false;
const pollTraffic = async () => {
    try {
        await loadTraffic();
    } catch (error) {
        console.error('Failed to read the aircraft', error);
    }
    if (isPolling) {
        pollTimer = setTimeout(pollTraffic, POLL_INTERVAL_MS);
    }
};

// Read the settings (once: the store keeps them across pages), showing the busy indicator meanwhile
const readConfig = async () => {
    try {
        setBusy(true, { label: t('common.syncingDevice') });
        await loadRadarConfig();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('app.loadError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Read the settings and start reading the aircraft when the page opens, stop when it closes
onMounted(async () => {
    if (!globalStore.value.configLoaded) {
        await readConfig();
    }
    isPolling = true;
    pollTraffic();
});
onBeforeUnmount(() => {
    isPolling = false;
    clearTimeout(pollTimer);
});

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.radar.title')">
        <!-- The settings couldn't be read: offer to try again -->
        <EmptyStateCard v-if="globalStore.configLoadFailed && !globalStore.configLoaded" :icon="Alert02Icon" :title="t('pages.radar.loadErrorTitle')" :description="t('pages.radar.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="readConfig">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('pages.radar.loadRetryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the settings from the device -->
        <LoadingStateCard v-else-if="!globalStore.configLoaded" :title="t('common.syncingDevice')" />

        <!-- No location yet: the radar has no center -->
        <EmptyStateCard v-else-if="!config.hasLocation" :icon="Location01Icon" :title="t('pages.radar.noLocationTitle')" :description="t('pages.radar.noLocationDescription')">
            <template #action>
                <Button variant="primary" @click="navigateTo(localePath('/settings'))">
                    <HugeiconsIcon :icon="Location01Icon" class="size-4" />
                    {{ t('pages.radar.noLocationAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- The radar -->
        <template v-else>
            <Card padding="none" class="overflow-hidden">
                <div class="bg-[#020403] p-3 sm:p-5">
                    <RadarScope v-if="traffic" :traffic="traffic" :received-at="globalStore.trafficReceivedAt" :config="config" :selected-hex="selectedHex" @select="handleRadarSelect" />
                    <div v-else class="flex aspect-square w-full items-center justify-center text-sm text-white/60">{{ t('pages.radar.status.loading') }}</div>
                </div>

                <!-- The aircraft picked on the radar -->
                <button v-if="selectedAircraft" type="button" class="flex w-full items-center gap-3 border-t border-border px-4 py-3 text-left hover:bg-accent" @click="isDetailsOpen = true">
                    <span class="size-3 shrink-0 rounded-full" :style="{ backgroundColor: getAltitudeColor(selectedAircraft) }" />
                    <span class="min-w-0 flex-1">
                        <span class="block truncate font-semibold text-foreground">{{ getAircraftName(selectedAircraft) }}</span>
                        <span class="block truncate text-xs text-muted-foreground">{{ describeAltitude(selectedAircraft) }} · {{ describeDistance(selectedAircraft) }}</span>
                    </span>
                    <span class="text-xs text-muted-foreground">{{ t('pages.radar.detailsAction') }}</span>
                </button>

                <!-- The state of the data, and the legend of the colors -->
                <div class="space-y-3 border-t border-border px-4 py-3">
                    <p class="text-sm text-foreground" aria-live="polite">{{ statusText }}</p>
                    <ul class="flex flex-wrap gap-x-3 gap-y-1 text-xs text-muted-foreground" :aria-label="t('pages.radar.legendLabel')">
                        <li v-for="band in legend" :key="band.label" class="flex items-center gap-1.5">
                            <span class="size-2.5 rounded-full" :style="{ backgroundColor: band.color }" />
                            {{ band.label }}
                        </li>
                    </ul>
                </div>
            </Card>

            <!-- The data has a problem -->
            <Alert v-if="hasDataProblem" color="yellow">
                <HugeiconsIcon :icon="Alert02Icon" />
                <AlertTitle>{{ t('pages.radar.problemTitle') }}</AlertTitle>
                <AlertDescription>{{ traffic.error || t('pages.radar.problemDescription') }}</AlertDescription>
            </Alert>
            <Alert v-else-if="globalStore.trafficLoadFailed" color="yellow">
                <HugeiconsIcon :icon="Alert02Icon" />
                <AlertTitle>{{ t('pages.radar.offlineTitle') }}</AlertTitle>
                <AlertDescription>{{ t('pages.radar.offlineDescription') }}</AlertDescription>
            </Alert>

            <!-- The nearest aircraft -->
            <ListGroup v-if="listedAircraft.length > 0" :title="t('pages.radar.listTitle')" :hint="t('pages.radar.listHint')">
                <ListRow v-for="item in listedAircraft" :key="item.hex" :label="item.name" :description="item.description" :value="item.value" :class="item.outOfRange ? 'opacity-60' : ''" action @click="handleListSelect(item.hex)">
                    <template #trailing>
                        <span class="size-2.5 shrink-0 rounded-full" :style="{ backgroundColor: item.color }" />
                    </template>
                </ListRow>
            </ListGroup>
        </template>

        <!-- AI agents: the MCP server and the guide they read -->
        <ListGroup :title="t('pages.radar.agents.title')" :hint="agentsHint">
            <ListRow :label="t('pages.radar.agents.mcpLabel')" :icon="RoboticIcon" :value="mcpUrl" wrap-value />
            <ListRow :label="t('pages.radar.agents.guideLabel')" :icon="BookOpen01Icon" action @click="handleAgentGuidePress" />
        </ListGroup>

        <!-- Details of an aircraft -->
        <ResponsiveDialog v-model:open="isDetailsOpen" :title="selectedAircraft ? getAircraftName(selectedAircraft) : t('pages.radar.detailsAction')" :description="selectedAircraft ? t('pages.radar.details.hex', { hex: selectedAircraft.hex.toUpperCase() }) : t('pages.radar.details.gone')">
            <ListGroup v-if="selectedAircraft">
                <ListRow v-for="row in selectedDetails" :key="row.label" :label="row.label" :icon="row.icon" :value="row.value" wrap-value />
            </ListGroup>
            <p v-else class="text-sm text-muted-foreground">{{ t('pages.radar.details.gone') }}</p>
            <template #footer>
                <Button v-if="selectedAircraft" variant="primary" @click="handleTrackerPress">
                    <HugeiconsIcon :icon="LinkSquare02Icon" class="size-4" />
                    {{ t('pages.radar.details.trackerAction') }}
                </Button>
            </template>
        </ResponsiveDialog>
    </GizmoPage>
</template>
