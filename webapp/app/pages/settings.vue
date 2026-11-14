<script setup>
import { computed, onMounted, ref, watch } from 'vue';
import AirportIcon from '@hugeicons/core-free-icons/AirportIcon';
import Alert02Icon from '@hugeicons/core-free-icons/Alert02Icon';
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import CheckmarkCircle02Icon from '@hugeicons/core-free-icons/CheckmarkCircle02Icon';
import Globe02Icon from '@hugeicons/core-free-icons/Globe02Icon';
import Location01Icon from '@hugeicons/core-free-icons/Location01Icon';
import PaletteIcon from '@hugeicons/core-free-icons/PaletteIcon';
import Radar02Icon from '@hugeicons/core-free-icons/Radar02Icon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import RotateClockwiseIcon from '@hugeicons/core-free-icons/RotateClockwiseIcon';
import Route01Icon from '@hugeicons/core-free-icons/Route01Icon';
import RulerIcon from '@hugeicons/core-free-icons/RulerIcon';
import SaveIcon from '@hugeicons/core-free-icons/SaveIcon';
import Sun03Icon from '@hugeicons/core-free-icons/Sun03Icon';
import Tag01Icon from '@hugeicons/core-free-icons/Tag01Icon';
import TextIcon from '@hugeicons/core-free-icons/TextIcon';
import Navigation03Icon from '@hugeicons/core-free-icons/Navigation03Icon';
import { HugeiconsIcon } from '@hugeicons/vue';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { Button } from '@brumaombra/ui-vintage/button';
import { EmptyStateCard } from '@brumaombra/ui-vintage/empty-state-card';
import { Input } from '@brumaombra/ui-vintage/input';
import { ListGroup, ListRow } from '@brumaombra/ui-vintage/list';
import { LoadingStateCard } from '@brumaombra/ui-vintage/loading-state-card';
import { showMessageDialog } from '@brumaombra/ui-vintage/message-dialog';
import { OptionPickerDialog } from '@brumaombra/ui-vintage/option-picker-dialog';
import { Slider } from '@brumaombra/ui-vintage/slider';
import { Switch } from '@brumaombra/ui-vintage/switch';
import { loadRadarConfig, saveRadarSettings } from '~/composables/useRadar.js';
import { BRIGHTNESS_MAX, BRIGHTNESS_MIN, LOCATION_NAME_MAX_LENGTH, RADAR_THEMES, RANGE_OPTIONS_KM, ROTATIONS, THEME_PALETTES, UNIT_SYSTEMS, formatCoordinates, formatRange, parseCoordinates } from '~/composables/useUtils.js';
import { useGlobalStore } from '~/composables/stores/useGlobalStore.js';

const globalStore = useGlobalStore();
const { t, locale } = useI18n();
const openPicker = ref(''); // Picker currently open ('range', 'units', 'theme', 'rotation', or none)

// The form: a copy of the settings read from the device, and the coordinates as typed
const form = ref(null);
const coordinatesText = ref('');
const isCoordinatesTouched = ref(false);

// Fill the form from the settings of the device (when they are read, and after a save)
const fillForm = () => {
    form.value = { ...globalStore.value.config };
    coordinatesText.value = formatCoordinates(globalStore.value.config);
    isCoordinatesTouched.value = false;
};
watch(() => globalStore.value.configLoaded, isLoaded => {
    if (isLoaded) fillForm();
}, { immediate: true });

// The coordinates typed, read (null when they aren't coordinates)
const parsedCoordinates = computed(() => parseCoordinates(coordinatesText.value));

// The coordinates are wrong once typed (an empty field is fine until there is a location to keep)
const isCoordinatesInvalid = computed(() => isCoordinatesTouched.value && coordinatesText.value.trim() !== '' && !parsedCoordinates.value);

// The name is too long for the device
const isNameTooLong = computed(() => new TextEncoder().encode(form.value?.locationName || '').length > LOCATION_NAME_MAX_LENGTH);

// Choices of the pickers
const rangeOptions = computed(() => RANGE_OPTIONS_KM.map(rangeKm => ({
    value: rangeKm,
    label: formatRange({ rangeKm, units: form.value.units, locale: locale.value }),
    description: t(`pages.settings.range.hint${rangeKm}`)
})));
const unitOptions = computed(() => UNIT_SYSTEMS.map(units => ({
    value: units,
    label: t(`pages.settings.units.${units}`),
    description: t(`pages.settings.units.${units}Meta`)
})));
const themeOptions = computed(() => RADAR_THEMES.map(theme => ({
    value: theme,
    label: t(`pages.settings.theme.${theme}`),
    description: t(`pages.settings.theme.${theme}Meta`),
    color: THEME_PALETTES[theme].sweep
})));
const rotationOptions = computed(() => ROTATIONS.map(rotation => ({
    value: rotation,
    label: `${rotation * 90}°`,
    description: t(`pages.settings.rotation.option${rotation}`)
})));

// Values shown in the rows
const rangeLabel = computed(() => formatRange({ rangeKm: form.value.rangeKm, units: form.value.units, locale: locale.value }));
const unitsLabel = computed(() => t(`pages.settings.units.${form.value.units}`));
const themeLabel = computed(() => t(`pages.settings.theme.${form.value.theme}`));
const rotationLabel = computed(() => `${form.value.rotation * 90}°`);

// Close a picker
const closePicker = open => {
    if (!open) openPicker.value = '';
};

// Set the brightness from the slider (it gives an array with one value)
const handleBrightnessUpdate = value => {
    form.value.brightness = value?.[0] ?? BRIGHTNESS_MIN;
};

// Load the settings again after a failed load
const handleRetryLoadPress = async () => {
    try {
        setBusy(true, { label: t('common.syncingDevice') });
        await loadRadarConfig();
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('app.loadError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Build what to send: every setting, with the coordinates when there are some
const buildPayload = () => {
    const payload = {
        locationName: form.value.locationName.trim(),
        rangeKm: form.value.rangeKm,
        units: form.value.units,
        theme: form.value.theme,
        brightness: form.value.brightness,
        rotation: form.value.rotation,
        showLabels: form.value.showLabels,
        showTrails: form.value.showTrails,
        showGroundTraffic: form.value.showGroundTraffic,
        showSweep: form.value.showSweep
    };
    if (parsedCoordinates.value) {
        payload.latitude = parsedCoordinates.value.latitude;
        payload.longitude = parsedCoordinates.value.longitude;
    }
    return payload;
};

// Save the settings (the display follows right away)
const handleSavePress = async () => {
    // Check the form first: the coordinates must be readable, and there must be some
    isCoordinatesTouched.value = true;
    if (!parsedCoordinates.value || isNameTooLong.value) {
        showMessageDialog({
            type: 'error',
            icon: Alert02Icon,
            title: t('common.errorTitle'),
            message: isNameTooLong.value ? t('pages.settings.location.nameTooLong') : t('pages.settings.location.coordinatesInvalid'),
            closeText: t('common.close'),
            closeButtonIcon: Cancel01Icon
        });
        return;
    }

    // Send it
    try {
        setBusy(true);
        await saveRadarSettings(buildPayload());
        fillForm();
        showMessageDialog({
            type: 'success',
            icon: CheckmarkCircle02Icon,
            title: t('pages.settings.saveSuccessTitle'),
            message: t('pages.settings.saveSuccess'),
            closeText: t('common.close'),
            closeButtonIcon: Cancel01Icon
        });
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.settings.saveError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Read the settings when the page opens (once: the store keeps them across pages)
onMounted(async () => {
    if (!globalStore.value.configLoaded) {
        await handleRetryLoadPress();
    }
});

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.settings.title')">
        <!-- The settings couldn't be read: offer to try again (a form here would save the defaults over the real settings) -->
        <EmptyStateCard v-if="globalStore.configLoadFailed && !globalStore.configLoaded" :icon="Alert02Icon" :title="t('pages.settings.loadErrorTitle')" :description="t('pages.settings.loadErrorDescription')">
            <template #action>
                <Button variant="primary" @click="handleRetryLoadPress">
                    <HugeiconsIcon :icon="RefreshIcon" class="size-4" />
                    {{ t('pages.settings.loadRetryAction') }}
                </Button>
            </template>
        </EmptyStateCard>

        <!-- Still reading the settings from the device -->
        <LoadingStateCard v-else-if="!form" :title="t('common.syncingDevice')" />

        <!-- Settings form -->
        <form v-else class="space-y-6" novalidate @submit.prevent="handleSavePress">
            <!-- Location: the center of the radar -->
            <ListGroup :title="t('pages.settings.location.title')" :hint="t('pages.settings.location.hint')">
                <ListRow :label="t('pages.settings.location.coordinates')" :icon="Location01Icon" label-for="coordinates">
                    <Input id="coordinates" v-model="coordinatesText" inputmode="decimal" autocomplete="off" placeholder="45.46420, 9.19000" :aria-invalid="isCoordinatesInvalid || undefined" @blur="isCoordinatesTouched = true" />
                    <p v-if="isCoordinatesInvalid" class="mt-2 text-xs text-destructive">{{ t('pages.settings.location.coordinatesInvalid') }}</p>
                </ListRow>
                <ListRow :label="t('pages.settings.location.name')" :icon="Tag01Icon" label-for="location-name">
                    <Input id="location-name" v-model="form.locationName" autocomplete="off" :placeholder="t('pages.settings.location.namePlaceholder')" :maxlength="LOCATION_NAME_MAX_LENGTH" :aria-invalid="isNameTooLong || undefined" />
                </ListRow>
            </ListGroup>

            <!-- Radar: how far it looks, and what it shows -->
            <ListGroup :title="t('pages.settings.radarTitle')" :hint="t('pages.settings.radarHint')">
                <ListRow :label="t('pages.settings.range.label')" :icon="RulerIcon" :value="rangeLabel" action @click="openPicker = 'range'" />
                <ListRow :label="t('pages.settings.units.label')" :icon="Globe02Icon" :value="unitsLabel" action @click="openPicker = 'units'" />
                <ListRow :label="t('pages.settings.groundTraffic')" :icon="AirportIcon" label-for="show-ground">
                    <template #trailing>
                        <Switch id="show-ground" v-model="form.showGroundTraffic" />
                    </template>
                </ListRow>
            </ListGroup>

            <!-- Display: how the round screen looks -->
            <ListGroup :title="t('pages.settings.displayTitle')" :hint="t('pages.settings.displayHint')">
                <ListRow :label="t('pages.settings.theme.label')" :icon="PaletteIcon" :value="themeLabel" action @click="openPicker = 'theme'" />
                <ListRow :label="t('pages.settings.brightness')" :icon="Sun03Icon" :value="`${form.brightness}%`">
                    <Slider :model-value="[Number(form.brightness)]" :min="BRIGHTNESS_MIN" :max="BRIGHTNESS_MAX" :step="5" :thumb-label="t('pages.settings.brightness')" @update:model-value="handleBrightnessUpdate" />
                </ListRow>
                <ListRow :label="t('pages.settings.rotation.label')" :icon="RotateClockwiseIcon" :value="rotationLabel" action @click="openPicker = 'rotation'" />
                <ListRow :label="t('pages.settings.sweep')" :icon="Radar02Icon" label-for="show-sweep">
                    <template #trailing>
                        <Switch id="show-sweep" v-model="form.showSweep" />
                    </template>
                </ListRow>
                <ListRow :label="t('pages.settings.labels')" :icon="TextIcon" label-for="show-labels">
                    <template #trailing>
                        <Switch id="show-labels" v-model="form.showLabels" />
                    </template>
                </ListRow>
                <ListRow :label="t('pages.settings.trails')" :icon="Route01Icon" label-for="show-trails">
                    <template #trailing>
                        <Switch id="show-trails" v-model="form.showTrails" />
                    </template>
                </ListRow>
            </ListGroup>

            <!-- Save button -->
            <Button variant="primary" type="submit" class="w-full">
                <HugeiconsIcon :icon="SaveIcon" class="size-4" />
                {{ t('pages.settings.saveAction') }}
            </Button>
        </form>

        <!-- Pickers -->
        <template v-if="form">
            <OptionPickerDialog :open="openPicker === 'range'" :title="t('pages.settings.range.label')" :description="t('pages.settings.range.note')" :options="rangeOptions" :model-value="form.rangeKm" @update:model-value="value => form.rangeKm = value" @update:open="closePicker" />
            <OptionPickerDialog :open="openPicker === 'units'" :title="t('pages.settings.units.label')" :options="unitOptions" :model-value="form.units" @update:model-value="value => form.units = value" @update:open="closePicker" />
            <OptionPickerDialog :open="openPicker === 'theme'" :title="t('pages.settings.theme.label')" :options="themeOptions" :model-value="form.theme" @update:model-value="value => form.theme = value" @update:open="closePicker">
                <template #icon="{ option }">
                    <span class="flex size-8 shrink-0 items-center justify-center rounded-full bg-[#020403]" aria-hidden="true">
                        <HugeiconsIcon :icon="Navigation03Icon" class="size-4" :style="{ color: option.color }" />
                    </span>
                </template>
            </OptionPickerDialog>
            <OptionPickerDialog :open="openPicker === 'rotation'" :title="t('pages.settings.rotation.label')" :description="t('pages.settings.rotation.note')" :options="rotationOptions" :model-value="form.rotation" @update:model-value="value => form.rotation = value" @update:open="closePicker" />
        </template>
    </GizmoPage>
</template>
