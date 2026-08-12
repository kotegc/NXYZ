#pragma once
#include <LovyanGFX.hpp>
#include <AiEsp32RotaryEncoder.h>
#include "lgfx_config.h"
#include "display_config.h"
#include "uart_protocol.h"

class PhysicsModule {
public:
  virtual void init(LGFX* gfx, AiEsp32RotaryEncoder* encoder) = 0;
  virtual void stop() = 0;
  virtual void loop() = 0;        // called repeatedly by main loop
  virtual const char* name() = 0; // display name for menu

  // ── Event manifest ──────────────────────────────────────
  // Declares what this module can emit over the event bus (see
  // event_bus.h). Fixed-size, module-owned, no heap/std::string/
  // std::vector — events() returns a pointer to a static const array the
  // module defines itself. moduleId() must return a distinct
  // nxyz_protocol::ModuleId.
  virtual uint8_t                               moduleId() const = 0;
  virtual const nxyz_protocol::EventDescriptor*  events() const = 0;
  virtual uint8_t                                eventCount() const = 0;
};