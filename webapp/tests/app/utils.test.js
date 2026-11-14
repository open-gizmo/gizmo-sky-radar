import { describe, expect, it } from 'vitest';
import { estimatePosition, formatAltitude, formatCoordinates, formatDistance, formatRange, formatSpeed, formatVerticalRate, getAircraftName, getAltitudeColor, getCompassPoint, parseCoordinates, projectPosition } from '~/composables/useUtils.js';

// Numbers in the units of the radar: the same texts as the display (units.cpp)
describe('formatting', () => {
    // Altitudes: flight levels high up in aviation units, feet or meters otherwise, and the ground
    it('formats altitudes like the display', () => {
        expect(formatAltitude({ altitudeFt: 35950, units: 'aviation', locale: 'en' })).toBe('FL360');
        expect(formatAltitude({ altitudeFt: 12500, units: 'aviation', locale: 'en' })).toBe('12,500 ft');
        expect(formatAltitude({ altitudeFt: 12500, units: 'metric', locale: 'en' })).toBe('3,810 m');
        expect(formatAltitude({ altitudeFt: 40000, units: 'metric', locale: 'it' })).toBe('12.192 m');
        expect(formatAltitude({ onGround: true, units: 'aviation', locale: 'en' })).toBe('GND');
        expect(formatAltitude({ altitudeFt: null, units: 'aviation', locale: 'en' })).toBe('—');
    });

    // Distances, ranges and speeds, with a decimal under 10
    it('formats distances, ranges and speeds', () => {
        expect(formatDistance({ distanceKm: 9.26, units: 'aviation', locale: 'en' })).toBe('5.0 nm');
        expect(formatDistance({ distanceKm: 23.4, units: 'metric', locale: 'en' })).toBe('23 km');
        expect(formatRange({ rangeKm: 40, units: 'aviation', locale: 'en' })).toBe('22 nm');
        expect(formatRange({ rangeKm: 5, units: 'aviation', locale: 'en' })).toBe('2.7 nm');
        expect(formatRange({ rangeKm: 40, units: 'metric', locale: 'en' })).toBe('40 km');
        expect(formatSpeed({ speedKt: 450, units: 'imperial', locale: 'en' })).toBe('518 mph');
        expect(formatSpeed({ speedKt: null, units: 'aviation', locale: 'en' })).toBe('—');
        expect(formatVerticalRate({ verticalRateFpm: 1200, units: 'aviation', locale: 'en' })).toBe('+1,200 ft/min');
        expect(formatVerticalRate({ verticalRateFpm: -600, units: 'metric', locale: 'en' })).toBe('-3.0 m/s');
    });

    // Names and colors of the aircraft, and compass points
    it('names and colors the aircraft', () => {
        expect(getAircraftName({ hex: 'abc123', callsign: 'SWR2KT', registration: 'HB-JDH' })).toBe('SWR2KT');
        expect(getAircraftName({ hex: 'abc123', callsign: null, registration: 'HB-JDH' })).toBe('HB-JDH');
        expect(getAircraftName({ hex: 'abc123', callsign: null, registration: null })).toBe('ABC123');
        expect(getAltitudeColor({ altitudeFt: 1500 })).toBe('#ff5e4d');
        expect(getAltitudeColor({ altitudeFt: 45000 })).toBe('#c58cff');
        expect(getAltitudeColor({ onGround: true })).toBe('#9a9a9a');
        expect(getCompassPoint(225)).toBe('SW');
        expect(getCompassPoint(-45)).toBe('NW');
    });
});

// Coordinates typed or pasted in the location field
describe('coordinates', () => {
    // The ways maps and people write them
    it.each([
        ['45.4642, 9.19', { latitude: 45.4642, longitude: 9.19 }],
        ['45.4642,9.19', { latitude: 45.4642, longitude: 9.19 }],
        [' -33.8688 151.2093 ', { latitude: -33.8688, longitude: 151.2093 }],
        ['51.47;-0.4543', { latitude: 51.47, longitude: -0.4543 }]
    ])('reads %s', (text, coordinates) => {
        expect(parseCoordinates(text)).toEqual(coordinates);
    });

    // Text that isn't coordinates, or coordinates off the Earth
    it.each(['', 'Milan', '45.46', '91, 9', '45, 181', '45.4642, 9.19, 3'])('refuses %s', text => {
        expect(parseCoordinates(text)).toBeNull();
    });

    // The field shows them with 5 decimals
    it('writes them back', () => {
        expect(formatCoordinates({ latitude: 45.4642, longitude: 9.19 })).toBe('45.46420, 9.19000');
        expect(formatCoordinates({ latitude: null, longitude: null })).toBe('');
    });
});

// Geography of the radar: the same as the display
describe('geography', () => {
    // An aircraft moves on along its track, up to 45 seconds; on the ground it stays
    it('moves an aircraft on along its track', () => {
        const aircraft = { latitude: 45, longitude: 9, groundSpeedKt: 360, trackDegrees: 0, onGround: false };
        expect(estimatePosition(aircraft, 10).latitude).toBeCloseTo(45 + 1.852 / 111.2, 3);
        expect(estimatePosition(aircraft, 600).latitude).toBeCloseTo(estimatePosition(aircraft, 45).latitude, 6);
        expect(estimatePosition({ ...aircraft, onGround: true }, 10)).toEqual({ latitude: 45, longitude: 9 });
    });

    // A point at the range due east is on the right of the outer ring
    it('projects a position on the radar', () => {
        const center = { latitude: 0, longitude: 0 };
        const east = projectPosition({ latitude: 0, longitude: 40 / 111.32 }, center, 40);
        expect(east.x).toBeCloseTo(1, 3);
        expect(east.y).toBeCloseTo(0, 3);
        expect(projectPosition({ latitude: 20 / 110.574, longitude: 0 }, center, 40).y).toBeCloseTo(-0.5, 3);
    });
});
