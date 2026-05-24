#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ILI9341 _panel;
  lgfx::Bus_SPI       _bus;
public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host    = SPI2_HOST;
      cfg.spi_mode    = 0;
      cfg.freq_write  = 40000000;
      cfg.freq_read   = 16000000;
      cfg.pin_sclk    = 12;
      cfg.pin_mosi    = 11;
      cfg.pin_miso    = 13;
      cfg.pin_dc      = 8;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs          = 10;
      cfg.pin_rst         = 9;
      cfg.pin_busy        = -1;
      cfg.memory_width    = 240;
      cfg.memory_height   = 320;
      cfg.panel_width     = 240;
      cfg.panel_height    = 320;
      cfg.offset_x        = 0;
      cfg.offset_y        = 0;
      cfg.offset_rotation = 0;
      cfg.readable        = true;
      cfg.invert          = true;
      cfg.rgb_order       = false;
      cfg.dlen_16bit      = false;
      cfg.bus_shared      = false;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};