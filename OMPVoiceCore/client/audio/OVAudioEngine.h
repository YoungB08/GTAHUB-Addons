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
    bool Initialize();
    void Shutdown();
    bool StartCapture();
    void StopCapture();
    void SetMasterVolume(float volume);
    void SetMicrophoneVolume(float volume);
    void EnableSmoothing(bool enable) noexcept { smoothing_ = enable; }
    void EnableHighPass(bool enable) noexcept { highPassEnabled_ = enable; }
    void EnableNoiseSuppression(bool enable) noexcept { noiseSuppressionEnabled_ = enable; }
    void EnableAGC(bool enable) noexcept { agc_.SetEnabled(enable); }
    void SetTransmitting(bool transmitting) noexcept { transmitting_ = transmitting; }
    void SetLoopback(bool enabled) noexcept { loopback_ = enabled; }
    void SetFrameHandler(EncodedFrameHandler handler);
    void OnRemoteFrame(VoiceFrame frame);
    void Update();
    [[nodiscard]] bool IsCapturing() const noexcept { return capture_.IsCapturing(); }
    [[nodiscard]] float MicRms() const noexcept { return capture_.Rms(); }
    [[nodiscard]] float MicPeak() const noexcept { return capture_.Peak(); }
    [[nodiscard]] std::size_t RemoteStreamCount() const noexcept { return remoteStreams_.size(); }

private:
    struct RemoteStream
    {
        OVJitterBuffer jitter;
        OVOpusDecoder decoder;
        bool initialized{};
    };
    void ProcessLoop();
    void ProcessCapture();
    void ProcessRemote();
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
};
}
