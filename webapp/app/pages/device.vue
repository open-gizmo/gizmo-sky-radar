<script setup>
import Cancel01Icon from '@hugeicons/core-free-icons/Cancel01Icon';
import InformationCircleIcon from '@hugeicons/core-free-icons/InformationCircleIcon';
import RefreshIcon from '@hugeicons/core-free-icons/RefreshIcon';
import { setBusy } from '@brumaombra/ui-vintage/busy-indicator';
import { showConfirmDialog } from '@brumaombra/ui-vintage/confirm-dialog';
import { ListRow } from '@brumaombra/ui-vintage/list';
import { showMessageDialog } from '@brumaombra/ui-vintage/message-dialog';
import { resetSettings } from '~/composables/useDeviceApi.js';

const { t } = useI18n();

// Forget the settings and the saved network, then restart the device into its setup hotspot
const resetSavedSettings = async () => {
    try {
        setBusy(true);
        await resetSettings();
        showMessageDialog({
            type: 'info',
            icon: InformationCircleIcon,
            title: t('pages.device.resetInfoTitle'),
            message: t('pages.device.resetInfoMessage'),
            closeText: t('common.close'),
            closeButtonIcon: Cancel01Icon
        });
    } catch (error) {
        handleBackendErrors({ error, defaultMessage: t('pages.device.resetError'), showDialog: true });
    } finally {
        setBusy(false);
    }
};

// Ask for confirmation before resetting
const handleResetPress = () => {
    showConfirmDialog({
        icon: RefreshIcon,
        title: t('pages.device.resetConfirmTitle'),
        message: t('pages.device.resetConfirmMessage'),
        confirmText: t('pages.device.resetAction'),
        cancelText: t('common.cancel'),
        confirmButtonType: 'red',
        cancelButtonType: 'secondary',
        confirmButtonIcon: RefreshIcon,
        cancelButtonIcon: Cancel01Icon,
        onConfirm: resetSavedSettings
    });
};

// Page metadata
definePageMeta({
    layout: 'dashboard'
});
</script>

<template>
    <GizmoPage :title="t('pages.device.title')">
        <!-- Device info, network, restart and forget network (from the gizmo-common layer, it reads the device by itself). The reset of the radar, its own action, joins the destructive rows at the end -->
        <GizmoDeviceCard>
            <template #danger-rows>
                <ListRow :label="t('pages.device.resetAction')" :icon="RefreshIcon" action danger @click="handleResetPress" />
            </template>
        </GizmoDeviceCard>
    </GizmoPage>
</template>
