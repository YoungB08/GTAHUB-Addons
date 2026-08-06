#include "OVPacket.h"

#include <limits>

namespace ov
{
namespace
{
void WriteU16(std::vector<std::uint8_t>& out, std::uint16_t value)
{
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void WriteU32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
    for (unsigned int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

void WriteU64(std::vector<std::uint8_t>& out, std::uint64_t value)
{
    for (unsigned int shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}

bool ReadU16(const std::vector<std::uint8_t>& in, std::size_t& offset, std::uint16_t& value)
{
    if (offset + 2 > in.size()) return false;
    value = static_cast<std::uint16_t>(in[offset]) |
        static_cast<std::uint16_t>(in[offset + 1] << 8U);
    offset += 2;
    return true;
}

bool ReadU32(const std::vector<std::uint8_t>& in, std::size_t& offset, std::uint32_t& value)
{
    if (offset + 4 > in.size()) return false;
    value = 0;
    for (unsigned int shift = 0; shift < 32; shift += 8)
        value |= static_cast<std::uint32_t>(in[offset++]) << shift;
    return true;
}

bool ReadU64(const std::vector<std::uint8_t>& in, std::size_t& offset, std::uint64_t& value)
{
    if (offset + 8 > in.size()) return false;
    value = 0;
    for (unsigned int shift = 0; shift < 64; shift += 8)
        value |= static_cast<std::uint64_t>(in[offset++]) << shift;
    return true;
}

std::vector<std::uint8_t> Wrap(OVPacket type, const std::vector<std::uint8_t>& payload)
{
    if (payload.size() > std::numeric_limits<std::uint16_t>::max()) return {};
    std::vector<std::uint8_t> out;
    out.reserve(payload.size() + 3);
    out.push_back(static_cast<std::uint8_t>(type));
    WriteU16(out, static_cast<std::uint16_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}
}

std::optional<DecodedPacket> DecodeDatagram(const std::uint8_t* data, std::size_t size)
{
    if (data == nullptr || size < 3) return std::nullopt;
    const auto typeValue = data[0];
    if (typeValue > static_cast<std::uint8_t>(OVPacket::Pong)) return std::nullopt;
    const std::size_t payloadSize = static_cast<std::size_t>(data[1]) |
        (static_cast<std::size_t>(data[2]) << 8U);
    if (payloadSize != size - 3) return std::nullopt;
    DecodedPacket packet;
    packet.type = static_cast<OVPacket>(typeValue);
    packet.payload.assign(data + 3, data + size);
    return packet;
}

std::vector<std::uint8_t> SerializeHandshake(const Handshake& handshake)
{
    std::vector<std::uint8_t> payload;
    payload.reserve(14);
    WriteU32(payload, handshake.magic);
    WriteU16(payload, handshake.version);
    WriteU64(payload, handshake.uid);
    return Wrap(OVPacket::Handshake, payload);
}

std::optional<Handshake> ParseHandshake(const std::vector<std::uint8_t>& datagram)
{
    const auto packet = DecodeDatagram(datagram.data(), datagram.size());
    if (!packet || packet->type != OVPacket::Handshake || packet->payload.size() != 14) return std::nullopt;
    Handshake handshake;
    std::size_t offset = 0;
    if (!ReadU32(packet->payload, offset, handshake.magic) ||
        !ReadU16(packet->payload, offset, handshake.version) ||
        !ReadU64(packet->payload, offset, handshake.uid)) return std::nullopt;
    return handshake;
}

std::vector<std::uint8_t> SerializeVoiceFrame(const VoiceFrame& frame)
{
    if (frame.encoded.empty() || frame.encoded.size() > MAX_ENCODED_FRAME) return {};
    std::vector<std::uint8_t> payload;
    payload.reserve(frame.encoded.size() + 14);
    WriteU16(payload, frame.playerId);
    WriteU32(payload, frame.channelId);
    WriteU16(payload, frame.sequence);
    WriteU32(payload, frame.timestampMs);
    WriteU16(payload, static_cast<std::uint16_t>(frame.encoded.size()));
    payload.insert(payload.end(), frame.encoded.begin(), frame.encoded.end());
    return Wrap(OVPacket::VoiceData, payload);
}

std::optional<VoiceFrame> ParseVoiceFrame(const std::vector<std::uint8_t>& datagram)
{
    const auto packet = DecodeDatagram(datagram.data(), datagram.size());
    if (!packet || packet->type != OVPacket::VoiceData || packet->payload.size() < 15) return std::nullopt;
    VoiceFrame frame;
    std::size_t offset = 0;
    std::uint16_t encodedSize = 0;
    if (!ReadU16(packet->payload, offset, frame.playerId) ||
        !ReadU32(packet->payload, offset, frame.channelId) ||
        !ReadU16(packet->payload, offset, frame.sequence) ||
        !ReadU32(packet->payload, offset, frame.timestampMs) ||
        !ReadU16(packet->payload, offset, encodedSize)) return std::nullopt;
    if (encodedSize == 0 || encodedSize > MAX_ENCODED_FRAME || offset + encodedSize != packet->payload.size()) return std::nullopt;
    frame.encoded.assign(packet->payload.begin() + static_cast<std::ptrdiff_t>(offset), packet->payload.end());
    return frame;
}

std::vector<std::uint8_t> SerializeControl(OVPacket type, std::uint32_t value)
{
    if (type == OVPacket::Handshake || type == OVPacket::VoiceData) return {};
    std::vector<std::uint8_t> payload;
    WriteU32(payload, value);
    return Wrap(type, payload);
}
}
