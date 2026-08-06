#include "audio/OVBassApi.h"

#include <cstdint>
#include <iostream>

namespace
{
ov::client::BassDword OV_BASS_CALL SilenceCallback(ov::client::BassHandle, void* buffer,
                                                    ov::client::BassDword length, void*)
{
    if (buffer && length) {
        auto* samples = static_cast<std::int16_t*>(buffer);
        for (std::uint32_t i = 0; i < length / sizeof(std::int16_t); ++i) samples[i] = 0;
    }
    return length;
}
}

int main()
{
#ifdef _WIN32
    ov::client::OVBassApi bass;
    if (!bass.Load()) return 1;
    if (!bass.IsLoaded() || !bass.IsFxLoaded()) return 2;

    // Device 0 is BASS's no-sound output. It keeps this deterministic on CI
    // machines without an audio endpoint while exercising the real modules.
    if (!bass.InitOutput(48000, 0)) return 3;
    const auto source = bass.CreateDecodeStream(48000, 1, &SilenceCallback, nullptr);
    if (!source) return 4;
    const auto highPass = bass.AddHighPassFilter(source, 120.0F);
    if (!highPass) return 5;
    if (!bass.RemoveFx(source, highPass)) return 6;
    const auto tempo = bass.CreateFxTempo(source);
    if (!tempo) return 7;
    if (!bass.ChannelFree(tempo)) return 8;
    std::cout << "BASS and BASS FX runtime tests passed\n";
    return 0;
#else
    return 0;
#endif
}
