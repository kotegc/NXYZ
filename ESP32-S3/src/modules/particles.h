#pragma once
#include "../module_base.h"
#include "../display_config.h"

#define MAX_BODIES   100
#define RADIUS         8
#define SPRITE_SIZE   (RADIUS * 2 + 1)
#define DIAMETER      (RADIUS * 2)

struct Body {
  float x, y;
  float vx, vy;
  int   px, py;
  bool  alive;
};

namespace particle_events {
  enum : uint8_t { WALL_BOUNCE = 0, BODY_COLLISION = 1, EVENT_COUNT };
}

class ParticleModule : public PhysicsModule {
public:
  void            init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) override;
  void            stop() override;
  void            loop() override;
  const char*     name() override { return "PARTICLES"; }

  uint8_t                              moduleId() const override { return nxyz_protocol::MODULE_PARTICLES; }
  const nxyz_protocol::EventDescriptor* events() const override;
  uint8_t                              eventCount() const override { return particle_events::EVENT_COUNT; }

private:
  LGFX*                 _gfx;
  AiEsp32RotaryEncoder* _encoder;
  TaskHandle_t          _physicsTask = nullptr;

  Body       _bodies[MAX_BODIES];
  int        _numBodies    = 0;
  int        _targetBodies = 0;

  uint16_t   _spriteWhite[SPRITE_SIZE * SPRITE_SIZE];
  uint16_t   _spriteBlack[SPRITE_SIZE * SPRITE_SIZE];

  SemaphoreHandle_t _mutex = nullptr;

  // loop state
  int        _lastDrawnCount  = -1;
  uint32_t   _lastFPSVal      = 0;
  bool       _fpsDirty        = false;
  int        _lastEncoderValue = 0;
  uint32_t   _lastEncoderTime  = 0;

  void buildSprites();
  void spawnBody(int i);
  void killBody(int i);
  void killAllBodies();
  void drawCounter(int count);
  void drawFPS(uint32_t fps);

  static void physicsTaskFn(void* param);
};