#pragma once
#include <cstdint>

// UART wire protocol shared between the ESP32-S3 (sender) and DaisySeed
// (receiver) sides. This is the single source of truth for the packet
// format — both projects include this header instead of each keeping
// their own copy of the byte layout.

namespace nxyz_protocol {

constexpr uint8_t  SYNC_BYTE        = 0xFF;
constexpr uint8_t  PACKET_SIZE      = 7;
constexpr uint8_t  MAX_PAYLOAD_BYTE = 254; // never 255/SYNC_BYTE — keeps payload bytes distinguishable from a new packet's start
constexpr uint32_t BAUD_RATE        = 31250; // MIDI standard baud rate

struct CollisionPacket {
    uint8_t bodyIndex;
    float   speed;
    float   normX;
    float   normY;
    uint8_t collisionType;
    uint8_t numBodies;
};

inline uint8_t clampByte(int value) {
    if (value < 0) return 0;
    if (value > MAX_PAYLOAD_BYTE) return MAX_PAYLOAD_BYTE;
    return (uint8_t)value;
}

inline void encodePacket(uint8_t* out7, uint8_t bodyIndex, float speed, float normX, float normY, uint8_t collisionType, uint8_t numBodies) {
    out7[0] = SYNC_BYTE;
    out7[1] = bodyIndex;
    out7[2] = clampByte((int)(speed * 20.f));
    out7[3] = clampByte((int)(normX * 255.f));
    out7[4] = clampByte((int)(normY * 255.f));
    out7[5] = collisionType;
    out7[6] = clampByte((int)numBodies);
}

inline CollisionPacket decodePacket(const uint8_t* p7) {
    CollisionPacket pkt;
    pkt.bodyIndex     = p7[1];
    pkt.speed         = p7[2] / 254.0f;
    pkt.normX         = p7[3] / 254.0f;
    pkt.normY         = p7[4] / 254.0f;
    pkt.collisionType = p7[5];
    pkt.numBodies     = p7[6];
    return pkt;
}

} // namespace nxyz_protocol
