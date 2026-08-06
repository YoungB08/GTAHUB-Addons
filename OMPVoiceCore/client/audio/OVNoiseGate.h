#pragma once

#include <cstdint>
#include <vector>

namespace ov::client
{
class OVNoiseGate final
{
public:
    explicit OVNoiseGate(float threshold = 0.015F) : threshold_(threshold) {}
    bool Process(std::vector<std::int16_t>& pcm);
    void SetThreshold(float threshold) noexcept { threshold_ = threshold; }

private:
    float threshold_;
};
}
