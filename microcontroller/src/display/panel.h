#ifndef PANEL_H
#define PANEL_H

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "../config/pins.h"

// Size of the GC9A01 round display, in pixels
constexpr int DISPLAY_SIZE = 240;

// SPI clock of the display (the GC9A01 takes 80 MHz on short wires: a full frame goes out in about 12 ms)
constexpr uint32_t DISPLAY_SPI_FREQUENCY = 80000000;

// The GC9A01 display on its SPI bus, with the PWM backlight
class RadarPanel : public lgfx::LGFX_Device {
public:
	RadarPanel() {
		// SPI bus (write only: the display never answers, so it needs no MISO)
		{
			auto config = bus.config();
			config.spi_host = SPI2_HOST;
			config.spi_mode = 0;
			config.freq_write = DISPLAY_SPI_FREQUENCY;
			config.freq_read = 16000000;
			config.spi_3wire = true;
			config.use_lock = true;
			config.dma_channel = SPI_DMA_CH_AUTO;
			config.pin_sclk = DISPLAY_SCLK_PIN;
			config.pin_mosi = DISPLAY_MOSI_PIN;
			config.pin_miso = -1;
			config.pin_dc = DISPLAY_DC_PIN;
			bus.config(config);
			panel.setBus(&bus);
		}

		// Panel
		{
			auto config = panel.config();
			config.pin_cs = DISPLAY_CS_PIN;
			config.pin_rst = DISPLAY_RST_PIN;
			config.pin_busy = -1;
			config.memory_width = DISPLAY_SIZE;
			config.memory_height = DISPLAY_SIZE;
			config.panel_width = DISPLAY_SIZE;
			config.panel_height = DISPLAY_SIZE;
			config.offset_x = 0;
			config.offset_y = 0;
			config.offset_rotation = 0;
			config.readable = false;
			config.invert = true;
			config.rgb_order = false;
			config.dlen_16bit = false;
			config.bus_shared = false;
			panel.config(config);
		}

		// Backlight, dimmed with PWM
		{
			auto config = light.config();
			config.pin_bl = DISPLAY_BL_PIN;
			config.invert = false;
			config.freq = 12000;
			config.pwm_channel = 7;
			light.config(config);
			panel.setLight(&light);
		}

		setPanel(&panel);
	}

private:
	lgfx::Bus_SPI bus;
	lgfx::Panel_GC9A01 panel;
	lgfx::Light_PWM light;
};

#endif
