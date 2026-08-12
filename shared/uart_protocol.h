#pragma once
#include <cstdint>

// UART wire protocol shared between the ESP32-S3 (sender) and DaisySeed
// (receiver) sides. This is the single source of truth for the packet
// format — both projects include this header instead of each keeping
// their own copy of the byte layout.
//
// Generic named-event protocol: any module declares a small manifest of
// outputs (continuous, 0.0-1.0) and triggers (discrete, one-shot), and
// emits them through this one packet shape instead of each module
// hand-rolling its own wire format. See 01_Docs/Architecture/
// event_protocol_overview.md for how this fits together end to end.

namespace nxyz_protocol {

constexpr uint8_t  SYNC_BYTE        = 0xFF;
constexpr uint8_t  PACKET_SIZE      = 8;
constexpr uint8_t  MAX_PAYLOAD_BYTE = 254; // never 255/SYNC_BYTE — keeps payload bytes distinguishable from a new packet's start
constexpr uint32_t BAUD_RATE        = 31250; // MIDI standard baud rate

// ── Musical tick clock ──────────────────────────────────────
// No tempo UI/Transport exists yet — these are fixed constants, agreed on
// at compile time by both firmwares, used only to timestamp emitted
// events. TICK_WRAP bounds the 24-bit tick field carried on the wire;
// at the tick rate implied by NOMINAL_BPM/TICKS_PER_QUARTER_NOTE below,
// it wraps roughly every 24 hours, well outside any single bench session.
constexpr uint16_t TICKS_PER_QUARTER_NOTE = 96;    // PPQN
constexpr float    NOMINAL_BPM            = 120.0f;
constexpr float    TICK_PERIOD_MS         =
    60000.0f / (NOMINAL_BPM * TICKS_PER_QUARTER_NOTE);  // ~5.208 ms/tick
constexpr uint32_t TICK_WRAP              = 1u << 24;   // 24-bit wire field

enum ModuleId : uint8_t {
    MODULE_PARTICLES = 0,
    MODULE_PENDULUM  = 1,
    MODULE_CHLADNI   = 2,
    MODULE_COUNT
};

enum class EventKind : uint8_t { Output = 0, Trigger = 1 };

// Static, module-owned descriptor. ESP32-side only — never serialized;
// `id` is the value that goes on the wire as eventId. `name` is for
// logging/debug only.
struct EventDescriptor {
    uint8_t     id;
    EventKind   kind;
    const char* name;
};

struct EventPacket {
    uint8_t  moduleId;
    uint8_t  eventId;
    float    value;  // 0.0-1.0
    float    aux;    // 0.0-1.0, meaning is per-event; 0 if unused
    uint32_t tick;   // unwrapped tick count, from a 24-bit wire field
};

inline uint8_t clampByte(float v01) {
    if (v01 < 0.f) return 0;
    if (v01 > 1.f) return MAX_PAYLOAD_BYTE;
    return (uint8_t)(v01 * MAX_PAYLOAD_BYTE);
}

inline void encodeEventPacket(uint8_t* out8, uint8_t moduleId, uint8_t eventId,
                               float value, float aux, uint32_t tick) {
    uint32_t t = tick % TICK_WRAP;
    out8[0] = SYNC_BYTE;
    out8[1] = moduleId;
    out8[2] = eventId;
    out8[3] = clampByte(value);
    out8[4] = clampByte(aux);
    out8[5] = (uint8_t)((t >> 16) & 0xFF);
    out8[6] = (uint8_t)((t >> 8)  & 0xFF);
    out8[7] = (uint8_t)(t & 0xFF);
}

inline EventPacket decodeEventPacket(const uint8_t* p8) {
    EventPacket pkt;
    pkt.moduleId = p8[1];
    pkt.eventId  = p8[2];
    pkt.value    = p8[3] / (float)MAX_PAYLOAD_BYTE;
    pkt.aux      = p8[4] / (float)MAX_PAYLOAD_BYTE;
    pkt.tick     = ((uint32_t)p8[5] << 16) | ((uint32_t)p8[6] << 8) | (uint32_t)p8[7];
    return pkt;
}

} // namespace nxyz_protocol
