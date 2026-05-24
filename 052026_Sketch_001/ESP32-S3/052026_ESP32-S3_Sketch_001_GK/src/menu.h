#pragma once
#include <LovyanGFX.hpp>
#include <AiEsp32RotaryEncoder.h>
#include "module_base.h"

class Menu {
public:
  Menu(LGFX* gfx, AiEsp32RotaryEncoder* encoder);
  PhysicsModule* run(PhysicsModule** modules, int count);  // blocks until selection
private:
  LGFX*                  _gfx;
  AiEsp32RotaryEncoder*  _encoder;
  void draw(PhysicsModule** modules, int count, int selected);
};