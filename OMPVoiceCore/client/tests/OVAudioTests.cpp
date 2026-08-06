#include "audio/OVAutomaticGain.h"
#include "audio/OVHighPassFilter.h"
#include "audio/OVJitterBuffer.h"
#include "audio/OVNoiseGate.h"
#include "audio/OVMicCapture.h"
#include "audio/OVOpusDecoder.h"
#include "audio/OVOpusEncoder.h"
#include "debug/OVPacketSimulator.h"

#include "shared/OVConstants.h"

#include <algorithm>
#include <cmath>
#include <array>
#include <chrono>
#include <ctime>
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

    const auto encodeStart = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 100; ++iteration) if (encoder.Encode(pcm.data(), pcm.size()).empty()) return 5;
    const auto encodeElapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - encodeStart).count() / 100.0;
    const auto decodeStart = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < 100; ++iteration) if (decoder.Decode(encoded.data(), encoded.size(), false).size() != ov::FRAME_SAMPLES) return 6;
    const auto decodeElapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - decodeStart).count() / 100.0;
    if (encodeElapsed > 2.0 || decodeElapsed > 1.0) return 7;

    ov::client::OVJitterBuffer jitter;
    jitter.Push(10, encoded);
    jitter.Push(12, encoded);
    const auto first = jitter.Pop();
    const auto missing = jitter.Pop();
    const auto third = jitter.Pop();
    if (!first || !missing || !missing->encoded.empty() || !third) return 8;

    ov::client::OVJitterBuffer lossJitter;
    for (std::uint16_t sequence = 1; sequence <= 101; ++sequence)
        if (sequence % 20 != 0) lossJitter.Push(sequence, encoded);
    int recovered = 0;
    int concealed = 0;
    while (const auto packet = lossJitter.Pop())
    {
        ++recovered;
        if (packet->encoded.empty()) ++concealed;
    }
    if (recovered != 101 || concealed != 5) return 9;

    ov::client::OVNoiseGate gate(0.1F);
    std::vector<std::int16_t> silence(ov::FRAME_SAMPLES, 0);
    if (gate.Process(silence)) return 10;
    ov::client::OVHighPassFilter highPass;
    highPass.Process(pcm);
    ov::client::OVAutomaticGain gain;
    gain.Process(pcm);
    ov::client::OVPacketSimulator simulator;
    simulator.SetPacketLoss(0.0F);
    if (simulator.Drop()) return 11;
    simulator.SetJitter(25);
    for (int iteration = 0; iteration < 100; ++iteration) { const int jitterValue = simulator.Jitter(); if (jitterValue < -25 || jitterValue > 25) return 12; }
    simulator.SetLatency(100);
    if (simulator.Latency() != 100) return 13;
    if (ov::client::OVMicCapture::MeasureCallbackMicros(500) >= 5000.0F) return 14;
    ov::client::OVBassApi analysisBass;
    ov::client::OVMicCapture analysisCapture(analysisBass);
    analysisCapture.InjectSamplesForTesting(pcm);
    const auto waveform = analysisCapture.Waveform();
    if (analysisCapture.Rms() <= 0.1F || analysisCapture.Peak() <= analysisCapture.Rms() || waveform.size() != 128) return 15;
    if (*std::max_element(waveform.begin(), waveform.end()) <= 0.1F) return 16;

    constexpr int performanceFrames = 200;
    constexpr double simulatedSeconds = performanceFrames * 0.02;
    ov::client::OVOpusEncoder performanceEncoder;
    ov::client::OVOpusDecoder oneVoiceDecoder;
    if (!performanceEncoder.Initialize() || !oneVoiceDecoder.Initialize()) return 17;
    const std::clock_t oneVoiceStart = std::clock();
    for (int frame = 0; frame < performanceFrames; ++frame)
    {
        const auto packet = performanceEncoder.Encode(pcm.data(), pcm.size());
        if (packet.empty() || oneVoiceDecoder.Decode(packet.data(), packet.size(), false).size() != ov::FRAME_SAMPLES) return 18;
    }
    const double oneVoiceCpu = static_cast<double>(std::clock() - oneVoiceStart) / CLOCKS_PER_SEC / simulatedSeconds * 100.0;
    if (oneVoiceCpu >= 4.0) return 19;

    std::array<ov::client::OVOpusDecoder, 8> voiceDecoders;
    for (auto& voiceDecoder : voiceDecoders) if (!voiceDecoder.Initialize()) return 20;
    const std::clock_t eightVoiceStart = std::clock();
    for (int frame = 0; frame < performanceFrames; ++frame)
    {
        const auto packet = performanceEncoder.Encode(pcm.data(), pcm.size());
        if (packet.empty()) return 21;
        for (auto& voiceDecoder : voiceDecoders)
            if (voiceDecoder.Decode(packet.data(), packet.size(), false).size() != ov::FRAME_SAMPLES) return 22;
    }
    const double eightVoiceCpu = static_cast<double>(std::clock() - eightVoiceStart) / CLOCKS_PER_SEC / simulatedSeconds * 100.0;
    std::cout << "CPU budgets: one voice=" << oneVoiceCpu << "%, eight voices=" << eightVoiceCpu << "%\n";
    if (eightVoiceCpu >= 10.0) return 23;
    std::cout << "Audio codec, DSP, and CPU budget tests passed\n";
    return 0;
}
