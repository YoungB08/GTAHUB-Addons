#pragma once

#include <cstddef>
#include <cstdint>

namespace ov
{
constexpr std::uint16_t OMPVOICE_PORT = 7775;
constexpr std::uint32_t OMPVOICE_MAGIC = 0x504D564FU; // "OVMP" on the wire.
constexpr std::uint16_t OMPVOICE_PROTOCOL_VERSION = 0x0100;
constexpr std::uint64_t OMPVOICE_UID = 0xD6FEE4A6B0EA27A3ULL;
constexpr std::uint32_t SAMPLE_RATE = 48000;
constexpr std::uint32_t FRAME_DURATION_MS = 20;
constexpr std::size_t FRAME_SAMPLES = SAMPLE_RATE * FRAME_DURATION_MS / 1000;
constexpr std::size_t MAX_ENCODED_FRAME = 1276;
constexpr std::size_t MAX_SIMULTANEOUS_VOICES = 8;
constexpr float DEFAULT_VOICE_DISTANCE = 25.0F;
constexpr float GLOBAL_DISTANCE = -1.0F;
constexpr float PHONE_LEAK_RADIUS = 8.0F;
}
