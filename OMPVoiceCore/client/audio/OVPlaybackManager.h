#pragma once

#include "OVBassApi.h"
#include "shared/OVThreadQueue.h"

#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>

namespace ov::client
{
class OVPlaybackManager final
{
public:
    explicit OVPlaybackManager(OVBassApi& bass);
    ~OVPlaybackManager();
    bool Initialize(int outputDevice = -1);
    void Shutdown();
    void PushMono(const std::vector<std::int16_t>& pcm, float gain, float pan);
    void SetMasterVolume(float volume);
    void SetSmoothing(bool enabled) noexcept;
    [[nodiscard]] bool IsReady() const noexcept { return stream_ != 0; }
    [[nodiscard]] std::uint64_t PlayedSamples() const noexcept { return playedSamples_.load(std::memory_order_relaxed); }

private:
    static BassDword OV_BASS_CALL OnStream(BassHandle handle, void* buffer, BassDword length, void* user);
    OVBassApi& bass_;
    BassHandle stream_{};
    std::mutex mutex_;
    std::vector<std::int16_t> queue_;
    std::size_t readOffset_{};
    float masterVolume_{0.32F};
    float currentVolume_{0.32F};
    bool smoothing_{true};
    std::atomic_uint64_t playedSamples_{};
};
}
