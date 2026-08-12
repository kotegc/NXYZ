#include "ticks.h"
#include <Arduino.h>
#include "uart_protocol.h"

void ticks::init() {
  // No-op for now; present so setup()'s call site doesn't need to change
  // if this later becomes hardware-timer-driven.
}

uint32_t ticks::now() {
  // millis() is safe to call from any core/task with no locking, which
  // matters because ParticleModule's physics task runs pinned to core 0,
  // outside the main Arduino loop.
  return (uint32_t)((float)millis() / nxyz_protocol::TICK_PERIOD_MS);
}
