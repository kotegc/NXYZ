#pragma once
#include "../module_base.h"
#include "../display_config.h"
#include <LovyanGFX.hpp>

#define BOB_RADIUS     4
#define ANCHOR_RADIUS  8

class PendulumModule : public PhysicsModule {
public:
  void        init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) override;
  void        stop() override;
  void        loop() override;
  const char* name() override { return "DOUBLE PENDULUM"; }

private:
  LGFX*                 _gfx;
  AiEsp32RotaryEncoder* _encoder;

  static constexpr float L1      = 55.0f;
  static constexpr float L2      = 50.0f;
  static constexpr float M1      = 1.0f;
  static constexpr float M2      = 1.0f;
  static constexpr float G       = 9.81f;
  static constexpr float DT      = 0.05f;
  static constexpr int   PIVOT_X = 160;
  static constexpr int   PIVOT_Y = 120;

  double _a1, _a2;
  double _v1, _v2;
  int    _x1, _y1, _x2, _y2;

  bool     _running    = false;
  bool     _firstFrame = true;
  bool     _fpsDirty   = false;
  uint32_t _lastFPSVal = 0;
  long     _lastEncoderRaw = 0;

  void computePositions();
  void drawAll();
  void eraseAll();
  void redrawTrail(int x0, int y0, int x1, int y1);
  void renderSprite(int oldX1, int oldY1, int oldX2, int oldY2,
                    int newX1, int newY1, int newX2, int newY2);
  void drawFPS(uint32_t fps);
  void stepPhysics();
};