#pragma once

#include <cstdint>
#include <vector>

namespace ov::client
{
class OVHighPassFilter final
{
public:
    explicit OVHighPassFilter(float cutoffHz = 120.0F);
    void Process(std::vector<std::int16_t>& pcm);
    void Reset() noexcept { previousInput_ = 0.0F; previousOutput_ = 0.0F; }

private:
    float coefficient_;
    float previousInput_{};
    float previousOutput_{};
};
}
