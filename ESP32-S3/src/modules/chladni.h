#pragma once
#include "../module_base.h"
#include "../display_config.h"
#include <LovyanGFX.hpp>

namespace chladni_events {
  enum : uint8_t { MODE_AMPLITUDE = 0, NODE_CROSSING = 1, EVENT_COUNT };
}

class ChladniModule : public PhysicsModule {
public:
  void        init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) override;
  void        stop() override;
  void        loop() override;
  const char* name() override { return "CHLADNI"; }

  uint8_t                              moduleId() const override { return nxyz_protocol::MODULE_CHLADNI; }
  const nxyz_protocol::EventDescriptor* events() const override;
  uint8_t                              eventCount() const override { return chladni_events::EVENT_COUNT; }

private:
  LGFX*                 _gfx;
  AiEsp32RotaryEncoder* _encoder;
  LGFX_Sprite*          _sprite = nullptr;

  static constexpr int   PLATE_SIZE   = 320;
  static constexpr int   OFFSET_Y     = 40;
  static constexpr float THRESHOLD    = 0.08f;
  static constexpr int   MAX_POSITION = 200;

  int      _position         = 0;
  int      _lastEncoderValue = 0;
  bool     _needsRedraw      = true;
  uint32_t _lastFPSVal       = 0;
  bool     _fpsDirty         = false;
  bool     _prevCenterPositive = true;

  void drawPattern();
  void drawFPS(uint32_t fps);
  float sampleCenterValue(float t);
};