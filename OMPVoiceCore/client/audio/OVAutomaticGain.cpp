#include "OVAutomaticGain.h"

#include <algorithm>
#include <cmath>

namespace ov::client
{
OVAutomaticGain::OVAutomaticGain(float targetRms) : targetRms_(targetRms) {}
void OVAutomaticGain::Process(std::vector<std::int16_t>& pcm)
{
    if (!enabled_ || pcm.empty()) return;
    double sum = 0.0;
    for (const auto sample : pcm) sum += static_cast<double>(sample) * sample;
    const float rms = static_cast<float>(std::sqrt(sum / pcm.size()) / 32768.0);
    if (rms > 0.001F) gain_ = std::clamp(gain_ + (targetRms_ / rms - gain_) * 0.02F, 0.25F, 2.0F);
    for (auto& sample : pcm) sample = static_cast<std::int16_t>(std::clamp(static_cast<float>(sample) * gain_, -32768.0F, 32767.0F));
}
}
