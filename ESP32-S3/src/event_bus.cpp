#include "event_bus.h"

static HardwareSerial* s_uart = nullptr;
static constexpr uint8_t MAX_EVENTS_PER_MODULE = 4;
static uint32_t s_lastSentMs[nxyz_protocol::MODULE_COUNT][MAX_EVENTS_PER_MODULE] = {};

void event_bus::init(HardwareSerial* uart) {
  s_uart = uart;
}

void event_bus::emit(uint8_t moduleId, uint8_t eventId, float value, float aux, uint32_t tick) {
  if (!s_uart) return;
  uint8_t packet[nxyz_protocol::PACKET_SIZE];
  nxyz_protocol::encodeEventPacket(packet, moduleId, eventId, value, aux, tick);
  s_uart->write(packet, nxyz_protocol::PACKET_SIZE);
}

bool event_bus::emitOutputThrottled(uint8_t moduleId, uint8_t eventId, float value,
                                     uint32_t tick, uint16_t minIntervalMs) {
  uint32_t now = millis();
  uint32_t &last = s_lastSentMs[moduleId][eventId];
  if (now - last < minIntervalMs) return false;
  last = now;
  emit(moduleId, eventId, value, 0.f, tick);
  return true;
}
