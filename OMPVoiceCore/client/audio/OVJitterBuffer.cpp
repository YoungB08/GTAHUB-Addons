#include "OVJitterBuffer.h"

namespace ov::client
{
OVJitterBuffer::OVJitterBuffer(std::size_t targetPackets) : targetPackets_(targetPackets) {}
bool OVJitterBuffer::IsBefore(std::uint16_t first, std::uint16_t second) { return static_cast<std::int16_t>(first - second) < 0; }
void OVJitterBuffer::Push(std::uint16_t sequence, std::vector<std::uint8_t> encoded)
{
    if (encoded.empty() || packets_.find(sequence) != packets_.end()) return;
    if (!started_)
    {
        started_ = true;
        nextSequence_ = sequence;
    }
    if (IsBefore(sequence, nextSequence_)) return;
    packets_.emplace(sequence, std::move(encoded));
}
std::optional<JitterPacket> OVJitterBuffer::Pop()
{
    if (!started_) return std::nullopt;
    if (!primed_)
    {
        if (packets_.size() < targetPackets_) return std::nullopt;
        primed_ = true;
    }
    if (packets_.empty()) return std::nullopt;
    const auto it = packets_.find(nextSequence_);
    if (it == packets_.end()) { ++nextSequence_; return JitterPacket{static_cast<std::uint16_t>(nextSequence_ - 1), {}}; }
    JitterPacket packet{it->first, std::move(it->second)};
    packets_.erase(it);
    ++nextSequence_;
    return packet;
}
void OVJitterBuffer::Reset() { packets_.clear(); started_ = false; primed_ = false; nextSequence_ = 0; }
}
