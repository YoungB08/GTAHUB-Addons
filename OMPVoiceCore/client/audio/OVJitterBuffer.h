#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace ov::client
{
struct JitterPacket { std::uint16_t sequence{}; std::vector<std::uint8_t> encoded; };

class OVJitterBuffer final
{
public:
    explicit OVJitterBuffer(std::size_t targetPackets = 2);
    void Push(std::uint16_t sequence, std::vector<std::uint8_t> encoded);
    std::optional<JitterPacket> Pop();
    void Reset();
    [[nodiscard]] std::size_t Size() const noexcept { return packets_.size(); }
    [[nodiscard]] bool NeedsPlc() const noexcept { return started_ && packets_.empty(); }

private:
    static bool IsBefore(std::uint16_t first, std::uint16_t second);
    std::size_t targetPackets_;
    bool started_{};
    bool primed_{};
    std::uint16_t nextSequence_{};
    std::map<std::uint16_t, std::vector<std::uint8_t>> packets_;
};
}
