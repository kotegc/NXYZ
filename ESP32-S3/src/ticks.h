#pragma once
#include <cstdint>

// Monotonic musical-tick clock. millis()-derived, not a hardware-timer
// ISR — sufficient because ticks only timestamp/order events this pass,
// not drive sample-accurate audio scheduling. See
// 01_Docs/Onboarding/concept_glossary.md for why ticks exist at all.

namespace ticks {
  void     init();  // call once from main.cpp::setup()
  uint32_t now();    // monotonic tick count since boot
}
