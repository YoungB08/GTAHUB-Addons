#include "OVPlaybackManager.h"

#include "shared/OVConstants.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ov::client
{
OVPlaybackManager::OVPlaybackManager(OVBassApi& bass) : bass_(bass) {}
OVPlaybackManager::~OVPlaybackManager() { Shutdown(); }
bool OVPlaybackManager::Initialize(int outputDevice)
{
    if (stream_ != 0) return true;
    if (!bass_.InitOutput(SAMPLE_RATE, outputDevice)) return false;
    stream_ = bass_.CreateStream(SAMPLE_RATE, 2, &OVPlaybackManager::OnStream, this);
    if (stream_ == 0) return false;
    bass_.ChannelSetVolume(stream_, 1.0F);
    return bass_.ChannelPlay(stream_);
}
void OVPlaybackManager::Shutdown()
{
    if (stream_ != 0) bass_.ChannelFree(stream_);
    stream_ = 0;
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear(); readOffset_ = 0;
}
void OVPlaybackManager::SetMasterVolume(float volume)
{
    std::lock_guard<std::mutex> lock(mutex_);
    masterVolume_ = std::clamp(volume, 0.0F, 1.0F);
    if (!smoothing_) currentVolume_ = masterVolume_;
}
void OVPlaybackManager::SetSmoothing(bool enabled) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    smoothing_ = enabled;
    if (!smoothing_) currentVolume_ = masterVolume_;
}
void OVPlaybackManager::PushMono(const std::vector<std::int16_t>& pcm, float gain, float pan)
{
    if (pcm.empty()) return;
    const float left = std::sqrt(std::clamp(1.0F - pan, 0.0F, 2.0F) * 0.5F) * gain;
    const float right = std::sqrt(std::clamp(1.0F + pan, 0.0F, 2.0F) * 0.5F) * gain;
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.reserve(queue_.size() + pcm.size() * 2);
    for (const auto sample : pcm)
    {
        queue_.push_back(static_cast<std::int16_t>(std::clamp(static_cast<float>(sample) * left, -32768.0F, 32767.0F)));
        queue_.push_back(static_cast<std::int16_t>(std::clamp(static_cast<float>(sample) * right, -32768.0F, 32767.0F)));
    }
    if (queue_.size() - readOffset_ > SAMPLE_RATE * 2 * 2) readOffset_ = queue_.size() - SAMPLE_RATE * 2 * 2;
}
BassDword OV_BASS_CALL OVPlaybackManager::OnStream(BassHandle, void* buffer, BassDword length, void* user)
{
    auto* playback = static_cast<OVPlaybackManager*>(user);
    if (!playback || !buffer || length == 0) return 0;
    std::lock_guard<std::mutex> lock(playback->mutex_);
    const float delta = playback->masterVolume_ - playback->currentVolume_;
    playback->currentVolume_ = playback->smoothing_ ? playback->currentVolume_ + delta * 0.18F : playback->masterVolume_;
    const std::size_t available = playback->queue_.size() - std::min(playback->readOffset_, playback->queue_.size());
    const std::size_t bytes = std::min<std::size_t>(length, available * sizeof(std::int16_t));
    if (bytes > 0)
    {
        std::memcpy(buffer, playback->queue_.data() + playback->readOffset_, bytes);
        auto* samples = static_cast<std::int16_t*>(buffer);
        for (std::size_t index = 0; index < bytes / sizeof(std::int16_t); ++index)
            samples[index] = static_cast<std::int16_t>(std::clamp(static_cast<float>(samples[index]) * playback->currentVolume_, -32768.0F, 32767.0F));
    }
    if (bytes < length) std::memset(static_cast<std::uint8_t*>(buffer) + bytes, 0, length - bytes);
    playback->playedSamples_.fetch_add(bytes / sizeof(std::int16_t), std::memory_order_relaxed);
    playback->readOffset_ += bytes / sizeof(std::int16_t);
    if (playback->readOffset_ > 4096 && playback->readOffset_ * 2 > playback->queue_.size())
    {
        playback->queue_.erase(playback->queue_.begin(), playback->queue_.begin() + static_cast<std::ptrdiff_t>(playback->readOffset_));
        playback->readOffset_ = 0;
    }
    return length;
}
}
