#pragma once

#include "OVAutomaticGain.h"
#include "OVBassApi.h"
#include "OVHighPassFilter.h"
#include "OVJitterBuffer.h"
#include "OVMicCapture.h"
#include "OVNoiseGate.h"
#include "OVOpusDecoder.h"
#include "OVOpusEncoder.h"
#include "OVPlaybackManager.h"
#include "shared/OVPacket.h"
#include "shared/OVThreadQueue.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace ov::client
{
class OVAudioEngine final
{
public:
    using EncodedFrameHandler = std::function<void(std::vector<std::uint8_t>)>;
    explicit OVAudioEngine(OVBassApi& bass);
    ~OVAudioEngine();
    bool Initialize(int outputDevice = -1);
    void Shutdown();
    bool StartCapture();
    void StopCapture();
    void SetMasterVolume(float volume);
    void SetMicrophoneVolume(float volume);
    bool SetInputDevice(int device);
    void EnableSmoothing(bool enable) noexcept;
    void EnableHighPass(bool enable) noexcept;
    void EnableNoiseSuppression(bool enable) noexcept { noiseSuppressionEnabled_ = enable; }
    void EnableAGC(bool enable) noexcept { agc_.SetEnabled(enable); }
    void SetTransmitting(bool transmitting) noexcept { transmitting_ = transmitting; }
    void SetLoopback(bool enabled) noexcept { loopback_ = enabled; }
    void SetVoiceActivation(bool enabled, float threshold) noexcept { voiceActivationEnabled_ = enabled; voiceActivationThreshold_ = threshold; }
    void SetFrameHandler(EncodedFrameHandler handler);
    void OnRemoteFrame(VoiceFrame frame);
    void Update();
    bool InjectDebugCaptureFrame(std::vector<std::int16_t> pcm);
    [[nodiscard]] bool IsCapturing() const noexcept { return capture_.IsCapturing(); }
    [[nodiscard]] float MicRms() const noexcept { return capture_.Rms(); }
    [[nodiscard]] float MicPeak() const noexcept { return capture_.Peak(); }
    [[nodiscard]] std::vector<float> MicWaveform() const { return capture_.Waveform(); }
    [[nodiscard]] bool IsTransmitting() const noexcept { return transmitting_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::vector<int> RemotePlayerIds() const;
    [[nodiscard]] std::size_t RemoteStreamCount() const noexcept { return remoteStreamCount_.load(); }
    [[nodiscard]] std::uint64_t PlayedSamples() const noexcept { return playback_.PlayedSamples(); }

private:
    struct RemoteStream
    {
        OVJitterBuffer jitter;
        OVOpusDecoder decoder;
        OVHighPassFilter effectHighPass{300.0F};
        float lowPassState{};
        float gain{1.0F};
        float pan{};
        VoiceMode mode{VoiceMode::Proximity};
        std::chrono::steady_clock::time_point lastPacket{};
        bool initialized{};
    };
    void ProcessLoop();
    void ProcessCapture();
    bool ProcessCaptureFrame(std::vector<std::int16_t> pcm);
    void ProcessRemote();
    static void ApplyModeEffect(RemoteStream& stream, std::vector<std::int16_t>& pcm);
    OVBassApi& bass_;
    OVMicCapture capture_;
    OVOpusEncoder encoder_;
    OVOpusDecoder loopbackDecoder_;
    OVPlaybackManager playback_;
    OVNoiseGate noiseGate_;
    OVHighPassFilter highPass_;
    OVAutomaticGain agc_;
    ThreadQueue<VoiceFrame> remoteQueue_{128};
    std::unordered_map<int, RemoteStream> remoteStreams_;
    mutable std::mutex remoteMutex_;
    std::vector<int> remotePlayerIds_;
    std::atomic_size_t remoteStreamCount_{};
    std::vector<std::int16_t> captureBuffer_;
    EncodedFrameHandler frameHandler_;
    std::mutex handlerMutex_;
    std::thread audioThread_;
    std::atomic_bool running_{};
    std::atomic_bool transmitting_{};
    bool initialized_{};
    bool loopback_{};
    bool smoothing_{true};
    bool highPassEnabled_{true};
    bool noiseSuppressionEnabled_{true};
    float microphoneVolume_{1.0F};
    int inputDevice_{-1};
    bool voiceActivationEnabled_{};
    float voiceActivationThreshold_{0.15F};
    int activationFrames_{};
    int releaseFrames_{};
    bool voiceActive_{};
};
}
