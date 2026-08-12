#pragma once
#include <LovyanGFX.hpp>
#include <AiEsp32RotaryEncoder.h>
#include "lgfx_config.h"
#include "display_config.h"

class PhysicsModule {
public:
  virtual void init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) = 0;
  virtual void stop() = 0;
  virtual void loop() = 0;        // called repeatedly by main loop
  virtual const char* name() = 0; // display name for menu
};