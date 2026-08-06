#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace ov::client
{
class OVPacketSimulator final
{
public:
    void SetPacketLoss(float percent) noexcept { packetLoss_ = percent; }
    void SetJitter(int milliseconds) noexcept { jitterMs_ = milliseconds; }
    void SetLatency(int milliseconds) noexcept { latencyMs_ = milliseconds; }
    [[nodiscard]] bool Drop();
    [[nodiscard]] int Jitter() const;
    [[nodiscard]] int Latency() const noexcept { return latencyMs_; }

private:
    float packetLoss_{};
    int jitterMs_{};
    int latencyMs_{};
    mutable std::mt19937 random_{std::random_device{}()};
};
}
