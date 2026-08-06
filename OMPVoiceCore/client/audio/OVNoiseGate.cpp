#include "OVNoiseGate.h"

#include <cmath>

namespace ov::client
{
bool OVNoiseGate::Process(std::vector<std::int16_t>& pcm)
{
    double sum = 0.0;
    for (const auto sample : pcm) sum += static_cast<double>(sample) * sample;
    const float rms = pcm.empty() ? 0.0F : static_cast<float>(std::sqrt(sum / pcm.size()) / 32768.0);
    if (rms >= threshold_) return true;
    std::fill(pcm.begin(), pcm.end(), 0);
    return false;
}
}
