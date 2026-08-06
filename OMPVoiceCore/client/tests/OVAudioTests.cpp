#include "audio/OVAutomaticGain.h"
#include "audio/OVHighPassFilter.h"
#include "audio/OVJitterBuffer.h"
#include "audio/OVNoiseGate.h"
#include "audio/OVOpusDecoder.h"
#include "audio/OVOpusEncoder.h"

#include "shared/OVConstants.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

int main()
{
    std::vector<std::int16_t> pcm(ov::FRAME_SAMPLES);
    for (std::size_t i = 0; i < pcm.size(); ++i) pcm[i] = static_cast<std::int16_t>(std::sin(static_cast<double>(i) * 0.04) * 12000.0);
    ov::client::OVOpusEncoder encoder;
    ov::client::OVOpusDecoder decoder;
    if (!encoder.Initialize() || !decoder.Initialize()) return 1;
    const auto encoded = encoder.Encode(pcm.data(), pcm.size());
    if (encoded.empty() || encoded.size() > ov::MAX_ENCODED_FRAME) return 2;
    const auto decoded = decoder.Decode(encoded.data(), encoded.size(), false);
    if (decoded.size() != ov::FRAME_SAMPLES) return 3;
    const auto plc = decoder.Decode(nullptr, 0, true);
    if (plc.size() != ov::FRAME_SAMPLES) return 4;

    ov::client::OVJitterBuffer jitter;
    jitter.Push(10, encoded);
    jitter.Push(12, encoded);
    const auto first = jitter.Pop();
    const auto missing = jitter.Pop();
    const auto third = jitter.Pop();
    if (!first || !missing || !missing->encoded.empty() || !third) return 5;

    ov::client::OVNoiseGate gate(0.1F);
    std::vector<std::int16_t> silence(ov::FRAME_SAMPLES, 0);
    if (gate.Process(silence)) return 7;
    ov::client::OVHighPassFilter highPass;
    highPass.Process(pcm);
    ov::client::OVAutomaticGain gain;
    gain.Process(pcm);
    std::cout << "Audio codec and DSP tests passed\n";
    return 0;
}
