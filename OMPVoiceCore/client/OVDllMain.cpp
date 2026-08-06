#include "audio/OVAudioEngine.h"
#include "audio/OVBassApi.h"
#include "config/OVClientConfig.h"
#include "debug/OVDiagnostic.h"
#include "hooks/OVGameHooks.h"
#include "network/OVNetworkClient.h"
#include "debug/OVPacketSimulator.h"
#include "render/OVDx9Renderer.h"
#include "shared/OVLogger.h"
#include "shared/OVCrashSafety.h"

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>

namespace ov::client
{
class ClientRuntime final
{
public:
    bool Initialize()
    {
        Logger::Instance().Initialize(std::filesystem::path("ompvoice") / "logs", "client.log", "Client");
        CrashSafety::Install(std::filesystem::path("ompvoice") / "debug");
        config_.Load();
        hooks_.Initialize();
        diagnostic_ = std::make_unique<OVDiagnostic>();
        renderer_ = std::make_shared<OVDx9Renderer>(config_, audio_, bass_, *diagnostic_);
        hooks_.InstallDx9Hooks(renderer_);
        audio_.SetFrameHandler([this](std::vector<std::uint8_t> encoded) {
            VoiceFrame frame;
            frame.playerId = playerId_;
            frame.channelId = 1;
            frame.sequence = sequence_++;
            frame.timestampMs = static_cast<std::uint32_t>(GetTickCount());
            frame.encoded = std::move(encoded);
            simulator_.SetPacketLoss(config_.Values().debug.packetLoss);
            if (simulator_.Drop()) return;
            const bool fakeRemote = fakeRemoteActive_.load(std::memory_order_relaxed);
            if (mirrorModeActive_.load(std::memory_order_relaxed) || fakeRemote)
            {
                VoiceFrame mirror = frame;
                mirror.playerId = 999;
                if (fakeRemote)
                {
                    const float phase = static_cast<float>(GetTickCount64() % 6000U) / 6000.0F * 6.2831853F;
                    mirror.pan = std::sin(phase) * 0.85F;
                    mirror.gain = 0.35F + 0.45F * (std::sin(phase * 0.5F) * 0.5F + 0.5F);
                }
                audio_.OnRemoteFrame(std::move(mirror));
            }
            network_.SendVoiceFrame(std::move(frame));
        });
        network_.Configure("127.0.0.1", OMPVOICE_PORT, playerId_);
        network_.SetFrameHandler([this](VoiceFrame frame) { audio_.OnRemoteFrame(std::move(frame)); });
        network_.Start();
        if (audio_.Initialize())
        {
            audio_.SetInputDevice(config_.Values().microphone.device);
            audio_.StartCapture();
        }
        running_ = true;
        worker_ = std::thread(&ClientRuntime::Loop, this);
        OV_LOG_INFO("Component", "ov_client.asi initialized for SA:MP 0.3.DL R1/open.mp");
        return true;
    }
    void Shutdown()
    {
        if (!running_.exchange(false)) return;
        if (worker_.joinable()) worker_.join();
        audio_.Shutdown();
        network_.Stop();
        hooks_.Shutdown();
        renderer_.reset();
        config_.Save();
        Logger::Instance().Shutdown();
        CrashSafety::Uninstall();
    }

private:
    void Loop()
    {
        bool transmitting = false;
        while (running_)
        {
            hooks_.PollHotkeys();
            if (hooks_.IsSettingsPressed() && renderer_) renderer_->ToggleSettings();
            if (hooks_.IsDebugPressed() && renderer_) renderer_->SetOverlayVisible(!config_.Values().debug.showOverlay);
            const bool shouldTransmit = hooks_.IsKeyDown(config_.Values().talkKey) && config_.Values().sound.enabled && config_.Values().microphone.enabled && !config_.Values().microphone.muted;
            if (shouldTransmit != transmitting)
            {
                transmitting = shouldTransmit;
                audio_.SetTransmitting(transmitting);
                if (transmitting) network_.SendVoiceBegin(1); else network_.SendVoiceEnd();
            }
            audio_.SetLoopback(hooks_.LoopbackToggled() || config_.Values().debug.loopback);
            audio_.SetVoiceActivation(config_.Values().sound.voiceActivation, config_.Values().sound.voiceThreshold);
            const bool fakeRemote = hooks_.FakeRemoteToggled() || config_.Values().debug.fakeRemote;
            fakeRemoteActive_.store(fakeRemote, std::memory_order_relaxed);
            mirrorModeActive_.store(config_.Values().debug.mirrorMode, std::memory_order_relaxed);
            if (renderer_) renderer_->SetFakeRemote(fakeRemote);
            audio_.SetMasterVolume(config_.Values().sound.masterVolume / 100.0F);
            audio_.EnableSmoothing(config_.Values().sound.smoothing);
            audio_.EnableHighPass(config_.Values().sound.highPassFilter);
            audio_.EnableNoiseSuppression(config_.Values().sound.noiseSuppression);
            audio_.EnableAGC(config_.Values().sound.automaticGainControl);
            audio_.SetMicrophoneVolume(config_.Values().microphone.gain);
            audio_.Update();
            hooks_.ClearEdgeEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (transmitting) network_.SendVoiceEnd();
    }

    std::atomic_bool running_{};
    std::atomic_bool fakeRemoteActive_{};
    std::atomic_bool mirrorModeActive_{};
    std::thread worker_;
    std::uint16_t playerId_{};
    std::uint16_t sequence_{};
    OVClientConfig config_;
    OVBassApi bass_;
    OVAudioEngine audio_{bass_};
    OVNetworkClient network_;
    OVPacketSimulator simulator_;
    OVGameHooks hooks_;
    std::unique_ptr<OVDiagnostic> diagnostic_;
    std::shared_ptr<OVDx9Renderer> renderer_;
};

std::unique_ptr<ClientRuntime> g_runtime;
std::atomic_bool g_processAttached{};

DWORD WINAPI RuntimeThread(void*)
{
    g_runtime = std::make_unique<ClientRuntime>();
    g_runtime->Initialize();
    while (g_processAttached) Sleep(100);
    g_runtime->Shutdown();
    g_runtime.reset();
    return 0;
}
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        ov::client::g_processAttached = true;
        HANDLE thread = CreateThread(nullptr, 0, &ov::client::RuntimeThread, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        ov::client::g_processAttached = false;
    }
    return TRUE;
}
