#include "OVAudioEngine.h"

#include "shared/OVConstants.h"
#include "shared/OVLogger.h"

#include <algorithm>
#include <chrono>

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
    if (!initialized_ || !capture_.Initialize(-1) || !capture_.Start()) return false;
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
        if (!transmitting_ || pcm.empty()) continue;
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
        auto& stream = remoteStreams_[frame->playerId];
        if (!stream.initialized) { stream.initialized = stream.decoder.Initialize(); }
        stream.jitter.Push(frame->sequence, std::move(frame->encoded));
        while (auto packet = stream.jitter.Pop())
        {
            const auto pcm = stream.decoder.Decode(packet->encoded.data(), packet->encoded.size(), packet->encoded.empty());
            playback_.PushMono(pcm, frame->gain, frame->pan);
        }
    }
}
}
