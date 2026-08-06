#include "OVAudioEngine.h"

#include "shared/OVConstants.h"
#include "shared/OVLogger.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace ov::client
{
OVAudioEngine::OVAudioEngine(OVBassApi& bass)
    : bass_(bass), capture_(bass), playback_(bass)
{
}
OVAudioEngine::~OVAudioEngine() { Shutdown(); }
bool OVAudioEngine::Initialize()
{
    if (initialized_) return true;
    if (!bass_.Load() || !encoder_.Initialize() || !loopbackDecoder_.Initialize() || !playback_.Initialize())
    {
        OV_LOG_ERROR("Audio", "Audio initialization failed: %s", bass_.LastError().c_str());
        return false;
    }
    initialized_ = true;
    return true;
}
void OVAudioEngine::Shutdown()
{
    StopCapture();
    playback_.Shutdown();
    encoder_.Shutdown();
    bass_.Unload();
    initialized_ = false;
}
bool OVAudioEngine::StartCapture()
{
    if (!initialized_ || !capture_.Initialize(inputDevice_) || !capture_.Start()) return false;
    if (!running_.exchange(true)) audioThread_ = std::thread(&OVAudioEngine::ProcessLoop, this);
    OV_LOG_INFO("Audio", "Capture started at %u Hz, 16-bit mono", SAMPLE_RATE);
    return true;
}
void OVAudioEngine::StopCapture()
{
    capture_.Stop();
    if (running_.exchange(false) && audioThread_.joinable()) audioThread_.join();
}
void OVAudioEngine::SetMasterVolume(float volume) { playback_.SetMasterVolume(std::clamp(volume, 0.0F, 1.0F)); }
void OVAudioEngine::SetMicrophoneVolume(float volume) { microphoneVolume_ = std::clamp(volume, 0.0F, 2.0F); }
bool OVAudioEngine::SetInputDevice(int device)
{
    inputDevice_ = device;
    if (!capture_.IsCapturing()) return true;
    StopCapture();
    return StartCapture();
}
void OVAudioEngine::SetFrameHandler(EncodedFrameHandler handler) { std::lock_guard<std::mutex> lock(handlerMutex_); frameHandler_ = std::move(handler); }
void OVAudioEngine::OnRemoteFrame(VoiceFrame frame) { remoteQueue_.Push(std::move(frame)); }
void OVAudioEngine::Update() { if (!running_) ProcessRemote(); }

void OVAudioEngine::ProcessLoop()
{
    while (running_)
    {
        ProcessCapture();
        ProcessRemote();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
void OVAudioEngine::ProcessCapture()
{
    auto frame = capture_.PopFrame();
    if (!frame) return;
    captureBuffer_.insert(captureBuffer_.end(), frame->begin(), frame->end());
    while (captureBuffer_.size() >= FRAME_SAMPLES)
    {
        std::vector<std::int16_t> pcm(captureBuffer_.begin(), captureBuffer_.begin() + FRAME_SAMPLES);
        captureBuffer_.erase(captureBuffer_.begin(), captureBuffer_.begin() + FRAME_SAMPLES);
        for (auto& sample : pcm) sample = static_cast<std::int16_t>(std::clamp(static_cast<float>(sample) * microphoneVolume_, -32768.0F, 32767.0F));
        if (highPassEnabled_) highPass_.Process(pcm);
        if (noiseSuppressionEnabled_) noiseGate_.Process(pcm);
        agc_.Process(pcm);
        const float rms = capture_.Rms();
        if (voiceActivationEnabled_)
        {
            if (rms >= voiceActivationThreshold_) { activationFrames_ = std::min(activationFrames_ + 1, 2); releaseFrames_ = 10; }
            else { activationFrames_ = 0; if (releaseFrames_ > 0) --releaseFrames_; }
            if (activationFrames_ >= 2) voiceActive_ = true;
            if (releaseFrames_ == 0) voiceActive_ = false;
        }
        else voiceActive_ = false;
        if ((!transmitting_ && !voiceActive_) || pcm.empty()) continue;
        auto encoded = encoder_.Encode(pcm.data(), pcm.size());
        if (encoded.empty()) continue;
        if (loopback_) playback_.PushMono(loopbackDecoder_.Decode(encoded.data(), encoded.size(), false), 1.0F, 0.0F);
        EncodedFrameHandler callback;
        { std::lock_guard<std::mutex> lock(handlerMutex_); callback = frameHandler_; }
        if (callback) callback(std::move(encoded));
    }
}
void OVAudioEngine::ProcessRemote()
{
    while (auto frame = remoteQueue_.TryPop())
    {
        if (remoteStreams_.find(frame->playerId) == remoteStreams_.end() && remoteStreams_.size() >= MAX_SIMULTANEOUS_VOICES)
        {
            const auto quietest = std::min_element(remoteStreams_.begin(), remoteStreams_.end(), [](const auto& left, const auto& right) { return left.second.gain < right.second.gain; });
            if (quietest != remoteStreams_.end()) remoteStreams_.erase(quietest);
        }
        auto& stream = remoteStreams_[frame->playerId];
        if (!stream.initialized) { stream.initialized = stream.decoder.Initialize(); }
        stream.gain = frame->gain;
        stream.pan = frame->pan;
        stream.mode = frame->mode;
        stream.lastPacket = std::chrono::steady_clock::now();
        stream.jitter.Push(frame->sequence, std::move(frame->encoded));
        while (auto packet = stream.jitter.Pop())
        {
            auto pcm = stream.decoder.Decode(packet->encoded.data(), packet->encoded.size(), packet->encoded.empty());
            ApplyModeEffect(stream, pcm);
            playback_.PushMono(pcm, stream.gain, stream.pan);
        }
    }
    const auto now = std::chrono::steady_clock::now();
    for (auto it = remoteStreams_.begin(); it != remoteStreams_.end();)
    {
        if (it->second.lastPacket.time_since_epoch().count() != 0 && now - it->second.lastPacket > std::chrono::seconds(1)) it = remoteStreams_.erase(it);
        else ++it;
    }
    remoteStreamCount_ = remoteStreams_.size();
}

void OVAudioEngine::ApplyModeEffect(RemoteStream& stream, std::vector<std::int16_t>& pcm)
{
    if (stream.mode != VoiceMode::Radio && stream.mode != VoiceMode::Phone) return;
    stream.effectHighPass.Process(pcm);
    const float cutoff = stream.mode == VoiceMode::Phone ? 3400.0F : 3000.0F;
    const float alpha = (2.0F * 3.14159265F * cutoff / static_cast<float>(SAMPLE_RATE)) /
        (1.0F + 2.0F * 3.14159265F * cutoff / static_cast<float>(SAMPLE_RATE));
    std::uint32_t noise = 0x9E3779B9U;
    for (auto& sample : pcm)
    {
        stream.lowPassState += alpha * (static_cast<float>(sample) - stream.lowPassState);
        float value = stream.lowPassState;
        value = value / (1.0F + std::abs(value) / 16000.0F) * 1.35F;
        if (stream.mode == VoiceMode::Radio)
        {
            noise = noise * 1664525U + 1013904223U;
            value += static_cast<float>(static_cast<int>((noise >> 24U) & 0xFFU) - 128) * 4.0F;
        }
        sample = static_cast<std::int16_t>(std::clamp(value, -32768.0F, 32767.0F));
    }
}
}
