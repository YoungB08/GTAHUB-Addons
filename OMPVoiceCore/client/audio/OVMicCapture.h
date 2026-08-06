#pragma once

#include "OVBassApi.h"
#include "shared/OVThreadQueue.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace ov::client
{
class OVMicCapture final
{
public:
    using FrameQueue = ThreadQueue<std::vector<std::int16_t>>;
    explicit OVMicCapture(OVBassApi& bass);
    ~OVMicCapture();
    bool Initialize(int device);
    bool Start();
    void Stop();
    [[nodiscard]] std::optional<std::vector<std::int16_t>> PopFrame();
    [[nodiscard]] bool IsCapturing() const noexcept { return recordHandle_ != 0; }
    [[nodiscard]] float Rms() const noexcept { return rms_.load(std::memory_order_relaxed); }
    [[nodiscard]] float Peak() const noexcept { return peak_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::vector<float> Waveform() const;
    static float MeasureCallbackMicros(std::size_t iterations);

private:
    static int OV_BASS_CALL OnRecord(BassHandle handle, const void* buffer, BassDword length, void* user);
    OVBassApi& bass_;
    FrameQueue frames_{32};
    BassHandle recordHandle_{};
    std::atomic<float> rms_{};
    std::atomic<float> peak_{};
    mutable std::mutex waveformMutex_;
    std::array<float, 128> waveform_{};
};
}
