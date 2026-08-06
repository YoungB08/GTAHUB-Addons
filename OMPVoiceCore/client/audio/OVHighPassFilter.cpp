#include "OVHighPassFilter.h"

#include "shared/OVConstants.h"

#include <algorithm>
#include <cmath>

namespace ov::client
{
OVHighPassFilter::OVHighPassFilter(float cutoffHz)
{
    const float rc = 1.0F / (2.0F * 3.14159265F * cutoffHz);
    coefficient_ = rc / (rc + 1.0F / static_cast<float>(SAMPLE_RATE));
}
void OVHighPassFilter::Process(std::vector<std::int16_t>& pcm)
{
    for (auto& sample : pcm)
    {
        const float input = static_cast<float>(sample);
        const float output = coefficient_ * (previousOutput_ + input - previousInput_);
        previousInput_ = input;
        previousOutput_ = output;
        sample = static_cast<std::int16_t>(std::clamp(output, -32768.0F, 32767.0F));
    }
}
}
