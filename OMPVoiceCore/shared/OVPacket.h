#pragma once

#include "OVConstants.h"
#include "OVTypes.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace ov
{
enum class OVPacket : std::uint8_t
{
    Handshake,
    VoiceBegin,
    VoiceData,
    VoiceEnd,
    Ping,
    Pong
};

struct PacketHeader
{
    OVPacket type{OVPacket::Handshake};
    std::uint16_t payloadSize{};
};

struct Handshake
{
    std::uint32_t magic{OMPVOICE_MAGIC};
    std::uint16_t version{OMPVOICE_PROTOCOL_VERSION};
    std::uint64_t uid{OMPVOICE_UID};
};

struct VoiceFrame
{
    std::uint16_t playerId{};
    std::uint32_t channelId{};
    std::uint16_t sequence{};
    std::uint32_t timestampMs{};
    float gain{1.0F};
    float pan{};
    VoiceMode mode{VoiceMode::Proximity};
    std::vector<std::uint8_t> encoded;
};

struct DecodedPacket
{
    OVPacket type{OVPacket::Handshake};
    std::vector<std::uint8_t> payload;
};

std::vector<std::uint8_t> SerializeHandshake(const Handshake& handshake);
std::optional<Handshake> ParseHandshake(const std::vector<std::uint8_t>& datagram);
std::vector<std::uint8_t> SerializeVoiceFrame(const VoiceFrame& frame);
std::optional<VoiceFrame> ParseVoiceFrame(const std::vector<std::uint8_t>& datagram);
std::vector<std::uint8_t> SerializeControl(OVPacket type, std::uint32_t value = 0);
std::optional<DecodedPacket> DecodeDatagram(const std::uint8_t* data, std::size_t size);
}
