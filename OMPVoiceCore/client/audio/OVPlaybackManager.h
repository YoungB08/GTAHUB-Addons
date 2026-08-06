#pragma once

#include "OVBassApi.h"
#include "shared/OVThreadQueue.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace ov::client
{
class OVPlaybackManager final
{
public:
    explicit OVPlaybackManager(OVBassApi& bass);
    ~OVPlaybackManager();
    bool Initialize();
    void Shutdown();
    void PushMono(const std::vector<std::int16_t>& pcm, float gain, float pan);
    void SetMasterVolume(float volume);
    [[nodiscard]] bool IsReady() const noexcept { return stream_ != 0; }

private:
    static BassDword OV_BASS_CALL OnStream(BassHandle handle, void* buffer, BassDword length, void* user);
    OVBassApi& bass_;
    BassHandle stream_{};
    std::mutex mutex_;
    std::vector<std::int16_t> queue_;
    std::size_t readOffset_{};
    float masterVolume_{0.32F};
};
}
