#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ov::client
{
struct DebugStats
{
    bool transmitting{};
    float micRms{};
    float micGain{1.0F};
    std::size_t encodedSize{};
    float bitrateKbps{};
    std::size_t jitterPackets{};
    float packetLoss{};
    std::size_t remoteStreams{};
    bool loopback{};
    bool fakeRemote{};
    bool mirrorMode{};
    std::uint32_t rttMs{};
};

class OVDebugOverlay final
{
public:
    void SetVisible(bool visible) noexcept { visible_ = visible; }
    void SetStats(DebugStats stats) noexcept { stats_ = stats; }
    void AddRms(float rms);
    void Render();

private:
    bool visible_{true};
    DebugStats stats_;
    std::vector<float> rmsHistory_;
};
}
