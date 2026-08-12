#pragma once
#include <cstdint>
#include <Arduino.h>
#include "uart_protocol.h"

// Shared UART sender for module event emission. Only one PhysicsModule is
// ever active at a time (main.cpp's activeModule pattern), and a module's
// stop() always finishes before the next module's init() runs, so writes
// here are never concurrent across modules — no mutex needed.

namespace event_bus {
  void init(HardwareSerial* uart);  // call once from main.cpp::setup()

  // Trigger events: always sent immediately, no rate limiting (discrete,
  // physically-gated by real events already).
  void emit(uint8_t moduleId, uint8_t eventId, float value, float aux, uint32_t tick);

  // Output events: physics/render loops can run at hundreds of Hz, far
  // faster than useful or affordable to transmit at 31250 baud. Drops
  // calls that arrive sooner than minIntervalMs after the last successful
  // send for this exact (moduleId, eventId) pair. Returns true if sent.
  bool emitOutputThrottled(uint8_t moduleId, uint8_t eventId, float value,
                            uint32_t tick, uint16_t minIntervalMs = 20);
}
