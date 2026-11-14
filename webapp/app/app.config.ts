import Radar01Icon from '@hugeicons/core-free-icons/Radar01Icon';
import Settings02Icon from '@hugeicons/core-free-icons/Settings02Icon';
import CpuIcon from '@hugeicons/core-free-icons/CpuIcon';

// Name, logo and pages of the app, read by the layout and the pages of the gizmo-common layer
export default defineAppConfig({
    gizmo: {
        name: 'Gizmo Sky Radar',

        // mDNS name of the firmware (http://gizmo-sky-radar.local)
        hostname: 'gizmo-sky-radar',

        logo: '/images/logo/gizmo-sky-radar-mark.svg',
        logoDark: '/images/logo/gizmo-sky-radar-mark-dark.svg',
        homePath: '/radar',

        // Pages of the dashboard, top to bottom in the sidebar and left to right in the bottom navigation ('wifi' is the page of the layer)
        nav: [
            { id: 'radar', labelKey: 'nav.radar.label', descriptionKey: 'nav.radar.description', icon: Radar01Icon, to: '/radar' },
            { id: 'settings', labelKey: 'nav.settings.label', descriptionKey: 'nav.settings.description', icon: Settings02Icon, to: '/settings' },
            { id: 'device', labelKey: 'nav.device.label', descriptionKey: 'nav.device.description', icon: CpuIcon, to: '/device' },
            'wifi'
        ]
    }
});
