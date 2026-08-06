#include "audio/OVBassApi.h"
#include "audio/OVMicCapture.h"

#include <Windows.h>
#include <Psapi.h>

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
    if (bass.RuntimeDirectory() != "ompvoice") return 20;

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

    auto captureCycle = [&bass]() {
        ov::client::OVMicCapture capture(bass);
        if (!capture.Initialize(-1) || !capture.Start()) return false;
        Sleep(10);
        capture.Stop();
        return !capture.IsCapturing();
    };
    for (int warmup = 0; warmup < 5; ++warmup) if (!captureCycle()) return 9;

    DWORD handlesBefore{};
    PROCESS_MEMORY_COUNTERS_EX memoryBefore{sizeof(memoryBefore)};
    if (!GetProcessHandleCount(GetCurrentProcess(), &handlesBefore)) return 10;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryBefore), sizeof(memoryBefore))) return 11;
    for (int iteration = 0; iteration < 50; ++iteration) if (!captureCycle()) return 12;
    DWORD handlesMidpoint{};
    PROCESS_MEMORY_COUNTERS_EX memoryMidpoint{sizeof(memoryMidpoint)};
    if (!GetProcessHandleCount(GetCurrentProcess(), &handlesMidpoint)) return 13;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryMidpoint), sizeof(memoryMidpoint))) return 14;
    for (int iteration = 50; iteration < 100; ++iteration) if (!captureCycle()) return 15;
    DWORD handlesAfter{};
    PROCESS_MEMORY_COUNTERS_EX memoryAfter{sizeof(memoryAfter)};
    if (!GetProcessHandleCount(GetCurrentProcess(), &handlesAfter)) return 16;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryAfter), sizeof(memoryAfter))) return 17;
    std::cout << "Capture handles: " << handlesBefore << " -> " << handlesMidpoint << " -> " << handlesAfter
              << ", private bytes: " << memoryBefore.PrivateUsage << " -> " << memoryMidpoint.PrivateUsage
              << " -> " << memoryAfter.PrivateUsage << '\n';
    if (handlesAfter > handlesMidpoint + 2) return 18;
    if (memoryAfter.PrivateUsage > memoryMidpoint.PrivateUsage + 2U * 1024U * 1024U) return 19;
    std::cout << "BASS, BASS FX, and 100-cycle capture tests passed\n";
    return 0;
#else
    return 0;
#endif
}
