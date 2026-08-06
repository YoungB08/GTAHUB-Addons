#pragma once

#include <cstdint>
#include <vector>

namespace ov::client
{
class OVAutomaticGain final
{
public:
    explicit OVAutomaticGain(float targetRms = 0.18F);
    void Process(std::vector<std::int16_t>& pcm);
    void SetEnabled(bool enabled) noexcept { enabled_ = enabled; }
    [[nodiscard]] float Gain() const noexcept { return gain_; }

private:
    float targetRms_;
    float gain_{1.0F};
    bool enabled_{true};
};
}
