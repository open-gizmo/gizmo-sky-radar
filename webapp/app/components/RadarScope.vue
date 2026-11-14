<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import { EMERGENCY_COLOR, THEME_PALETTES, estimatePosition, formatRange, getAircraftName, getAltitudeColor, projectPosition } from '~/composables/useUtils.js';

// Props: the last answer of /api/aircraft, when it arrived, the settings of the radar and the aircraft picked
const props = defineProps({
    traffic: { type: Object, required: true },
    receivedAt: { type: Number, required: true },
    config: { type: Object, required: true },
    selectedHex: { type: String, default: '' }
});

// Events: an aircraft was tapped
const emit = defineEmits(['select']);

const { locale } = useI18n();
const RIM_RADIUS = 1.1; // Aircraft past the outer ring up to here; farther ones get a marker on the rim
const SWEEP_SLICES = 20; // Slices of the trail of the beam
const SWEEP_TRAIL_DEGREES = 60;

// The clock that moves the aircraft on between two reads, like the display does
const now = ref(Date.now());
let clockTimer = null;
onMounted(() => {
    clockTimer = setInterval(() => {
        now.value = Date.now();
    }, 250);
});
onBeforeUnmount(() => clearInterval(clockTimer));

// Colors of the theme of the radar
const palette = computed(() => THEME_PALETTES[props.config.theme] || THEME_PALETTES.phosphor);

// The range of the outer ring, written in the units of the radar
const rangeLabel = computed(() => formatRange({ rangeKm: props.config.rangeKm, units: props.config.units, locale: locale.value }));

// The slices of the beam: thin wedges from the center, brighter toward the leading edge (the group turns with CSS)
const sweepSlices = computed(() => Array.from({ length: SWEEP_SLICES }, (_, index) => {
    const step = SWEEP_TRAIL_DEGREES / SWEEP_SLICES;
    const start = (-SWEEP_TRAIL_DEGREES + index * step) * Math.PI / 180;
    const end = (-SWEEP_TRAIL_DEGREES + (index + 1) * step + 0.3) * Math.PI / 180;
    return {
        path: `M0 0L${Math.sin(start)} ${-Math.cos(start)}A1 1 0 0 1 ${Math.sin(end)} ${-Math.cos(end)}Z`,
        opacity: ((index + 1) / SWEEP_SLICES) ** 2 * 0.45
    };
}));

// The ticks of the bezel: every 10 degrees, longer every 30
const ticks = Array.from({ length: 36 }, (_, index) => {
    const angle = index * 10 * Math.PI / 180;
    const length = index % 3 === 0 ? 0.06 : 0.035;
    return { x1: Math.sin(angle) * 1.02, y1: -Math.cos(angle) * 1.02, x2: Math.sin(angle) * (1.02 + length), y2: -Math.cos(angle) * (1.02 + length), isMajor: index % 3 === 0 };
});

// The compass points, outside the ring
const cardinals = [
    { label: 'N', x: 0, y: -1.13 },
    { label: 'E', x: 1.13, y: 0 },
    { label: 'S', x: 0, y: 1.13 },
    { label: 'W', x: -1.13, y: 0 }
];

// The aircraft as the radar draws them: where they are now, their trail, their color and label
const aircraft = computed(() => {
    const center = props.traffic.center;
    if (!center) return [];
    const elapsedSeconds = (now.value - props.receivedAt) / 1000;
    return (props.traffic.aircraft || []).map(item => {
        const position = projectPosition(estimatePosition(item, item.positionAgeSeconds + elapsedSeconds), center, props.config.rangeKm);
        const radius = Math.hypot(position.x, position.y);
        const color = item.emergency ? EMERGENCY_COLOR : getAltitudeColor(item);
        const trail = [...(item.trail || []).map(([latitude, longitude]) => projectPosition({ latitude, longitude }, center, props.config.rangeKm)), position];
        return {
            ...item,
            ...position,
            name: getAircraftName(item),
            color,
            isOnScreen: radius <= RIM_RADIUS,
            rimAngle: Math.atan2(position.x, -position.y) * 180 / Math.PI,
            trailPoints: trail.filter(point => Math.hypot(point.x, point.y) <= RIM_RADIUS).map(point => `${point.x.toFixed(4)},${point.y.toFixed(4)}`).join(' '),
            altitudeLabel: item.onGround ? 'GND' : (item.altitudeFt == null ? '' : String(Math.round((props.config.units === 'metric' ? item.altitudeFt * 0.3048 : item.altitudeFt) / 100)).padStart(3, '0'))
        };
    });
});

// Size of a label, in units of the radar: two lines of the monospace font (a character is about 0.6 of its size)
const LABEL_CHAR_WIDTH = 0.033;
const LABEL_HEIGHT = 0.12;

// Place the labels like the display: nearest aircraft first, on their right, else on their left, else none (never over another label)
const placeLabels = items => {
    const boxes = [];
    const placements = new Map();
    for (const item of items) {
        if (item.onGround) continue;
        const width = Math.max(item.name.length, item.altitudeLabel.length) * LABEL_CHAR_WIDTH;
        const options = [
            { x: item.x + 0.075, anchor: 'start', left: item.x + 0.07, right: item.x + 0.07 + width },
            { x: item.x - 0.075, anchor: 'end', left: item.x - 0.07 - width, right: item.x - 0.07 }
        ];
        const top = item.y - 0.07;
        const placement = options.find(option => !boxes.some(box => option.left < box.right && box.left < option.right && top < box.bottom && box.top < top + LABEL_HEIGHT));
        if (placement) {
            boxes.push({ left: placement.left, right: placement.right, top, bottom: top + LABEL_HEIGHT });
            placements.set(item.hex, placement);
        }
    }
    return placements;
};

// The aircraft on the radar, farthest first so the nearest are drawn on top, with the place of their label
const visibleAircraft = computed(() => {
    const onScreen = aircraft.value.filter(item => item.isOnScreen);
    const placements = placeLabels(onScreen);
    return onScreen.map(item => ({ ...item, label: placements.get(item.hex) || null })).reverse();
});

// The aircraft past the edge, as markers on the rim
const rimAircraft = computed(() => aircraft.value.filter(item => !item.isOnScreen));

// Pick an aircraft (tapping the picked one again lets it go)
const handleSelect = hex => {
    emit('select', hex === props.selectedHex ? '' : hex);
};
</script>

<template>
    <div class="radar-scope relative mx-auto aspect-square w-full max-w-md">
        <svg viewBox="-1.25 -1.25 2.5 2.5" class="size-full" role="img" :aria-label="`${aircraft.length} aircraft`">
            <defs>
                <radialGradient id="radar-glow">
                    <stop offset="0%" :stop-color="palette.background" />
                    <stop offset="100%" :stop-color="palette.backgroundEdge" />
                </radialGradient>
            </defs>

            <!-- The glass -->
            <circle r="1.24" fill="url(#radar-glow)" />

            <!-- The beam -->
            <g v-if="config.showSweep" class="radar-sweep">
                <path v-for="(slice, index) in sweepSlices" :key="index" :d="slice.path" :fill="palette.sweep" :fill-opacity="slice.opacity" />
                <line x1="0" y1="0" x2="0" y2="-1" :stroke="palette.sweep" stroke-width="0.012" />
            </g>

            <!-- The rings, the crosshair and the bezel -->
            <circle r="0.3333" fill="none" :stroke="palette.grid" stroke-width="0.008" />
            <circle r="0.6667" fill="none" :stroke="palette.grid" stroke-width="0.008" />
            <circle r="1" fill="none" :stroke="palette.gridBright" stroke-width="0.012" />
            <line x1="-1" y1="0" x2="1" y2="0" :stroke="palette.grid" stroke-width="0.006" stroke-dasharray="0.01 0.05" />
            <line x1="0" y1="-1" x2="0" y2="1" :stroke="palette.grid" stroke-width="0.006" stroke-dasharray="0.01 0.05" />
            <line v-for="(tick, index) in ticks" :key="index" :x1="tick.x1" :y1="tick.y1" :x2="tick.x2" :y2="tick.y2" :stroke="tick.isMajor ? palette.gridBright : palette.grid" stroke-width="0.01" />
            <text v-for="cardinal in cardinals" :key="cardinal.label" :x="cardinal.x" :y="cardinal.y" text-anchor="middle" dominant-baseline="central" font-size="0.09" font-weight="700" :fill="cardinal.label === 'N' ? palette.accent : palette.text">{{ cardinal.label }}</text>
            <text x="0.62" y="0.72" text-anchor="middle" font-size="0.06" :fill="palette.textDim">{{ rangeLabel }}</text>

            <!-- The aircraft past the edge, on the rim -->
            <g v-for="item in rimAircraft" :key="`rim-${item.hex}`" :transform="`rotate(${item.rimAngle})`">
                <path d="M0 -1.07L-0.03 -1.13L0.03 -1.13Z" :fill="item.color" fill-opacity="0.7" />
            </g>

            <!-- The trails -->
            <template v-if="config.showTrails">
                <polyline v-for="item in visibleAircraft" :key="`trail-${item.hex}`" :points="item.trailPoints" fill="none" :stroke="item.color" stroke-opacity="0.45" stroke-width="0.008" stroke-linejoin="round" />
            </template>

            <!-- The aircraft, turned to their track (a diamond without one), with their label -->
            <g v-for="item in visibleAircraft" :key="item.hex" class="cursor-pointer" :opacity="item.missing ? 0.5 : 1" @click="handleSelect(item.hex)">
                <circle :cx="item.x" :cy="item.y" r="0.08" fill="transparent" />
                <g :transform="`translate(${item.x} ${item.y}) rotate(${item.trackDegrees ?? 0}) scale(${item.onGround ? 0.0065 : 0.0085})`">
                    <path v-if="item.trackDegrees != null" d="M0 -7.5L1.4 5.5H-1.4ZM0 -2.5L7 2.2H-7ZM0 3L3.2 6.5H-3.2Z" :fill="item.color" />
                    <path v-else d="M0 -4L4 0L0 4L-4 0Z" :fill="item.color" />
                </g>
                <circle v-if="item.emergency" :cx="item.x" :cy="item.y" r="0.1" fill="none" :stroke="EMERGENCY_COLOR" stroke-width="0.012" class="radar-blink" />
                <g v-if="config.showLabels && item.label" :transform="`translate(${item.label.x} ${item.y - 0.02})`" :text-anchor="item.label.anchor">
                    <text font-size="0.055" font-family="ui-monospace, monospace" :fill="palette.text">{{ item.name }}</text>
                    <text y="0.06" font-size="0.05" font-family="ui-monospace, monospace" :fill="palette.textDim">{{ item.altitudeLabel }}</text>
                </g>
            </g>

            <!-- The aircraft picked: corner brackets around it -->
            <template v-for="item in visibleAircraft" :key="`selected-${item.hex}`">
                <path v-if="item.hex === selectedHex" :transform="`translate(${item.x} ${item.y})`" d="M-0.1 -0.06V-0.1H-0.06M0.06 -0.1H0.1V-0.06M0.1 0.06V0.1H0.06M-0.06 0.1H-0.1V0.06" fill="none" :stroke="palette.accent" stroke-width="0.012" />
            </template>

            <!-- The home of the radar -->
            <circle r="0.045" fill="none" :stroke="palette.gridBright" stroke-width="0.01" />
            <circle r="0.018" :fill="palette.accent" />
        </svg>
    </div>
</template>

<style scoped>
/* The beam goes round in 4 seconds, like on the display */
.radar-sweep {
    animation: radar-sweep 4s linear infinite;
    transform-origin: 0 0;
}

@keyframes radar-sweep {
    to {
        transform: rotate(360deg);
    }
}

/* An emergency blinks */
.radar-blink {
    animation: radar-blink 0.6s steps(2, jump-none) infinite;
}

@keyframes radar-blink {
    to {
        opacity: 0;
    }
}

/* No turning beam for people who asked for less motion */
@media (prefers-reduced-motion: reduce) {
    .radar-sweep,
    .radar-blink {
        animation: none;
    }
}
</style>
