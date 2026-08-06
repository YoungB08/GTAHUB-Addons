#include "audio/OVAudioEngine.h"
#include "audio/OVBassApi.h"
#include "network/OVNetworkClient.h"
#include "server/channels/OVChannelManager.h"
#include "server/voice/OVVoiceServer.h"
#include "shared/OVConstants.h"

#include <Windows.h>
#include <Psapi.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace
{
struct Snapshot
{
    DWORD handles{};
    std::size_t privateBytes{};
};

bool WaitFor(const std::function<bool()>& predicate, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return predicate();
}

bool ReadSnapshot(Snapshot& snapshot)
{
    PROCESS_MEMORY_COUNTERS_EX memory{sizeof(memory)};
    if (!GetProcessHandleCount(GetCurrentProcess(), &snapshot.handles)) return false;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
        return false;
    snapshot.privateBytes = memory.PrivateUsage;
    return true;
}

int Fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}
}

int main(int argc, char** argv)
{
    std::string mode = "ptt";
    int durationSeconds = 5;
    int port = ov::OMPVOICE_PORT;
    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument.rfind("--mode=", 0) == 0) mode = argument.substr(7);
        else if (argument.rfind("--duration=", 0) == 0) durationSeconds = std::atoi(argument.c_str() + 11);
        else if (argument.rfind("--port=", 0) == 0) port = std::atoi(argument.c_str() + 7);
        else return Fail("unknown command-line argument");
    }
    if ((mode != "ptt" && mode != "idle") || durationSeconds < 1 || durationSeconds > 86400)
        return Fail("invalid soak mode or duration");
    if (port < 1 || port > 65535) return Fail("invalid soak port");

    constexpr std::uint16_t speakerId = 21;
    constexpr std::uint16_t listenerId = 22;
    ov::server::OVChannelManager channels;
    channels.UpsertPlayer(speakerId, {}, -1);
    channels.UpsertPlayer(listenerId, {1.0F, 0.0F, 0.0F}, -1);
    ov::server::OVVoiceServer server(channels);
    const auto voicePort = static_cast<std::uint16_t>(port);
    if (!server.Start(voicePort)) return Fail("UDP voice server start");

    ov::client::OVNetworkClient speaker;
    ov::client::OVNetworkClient listener;
    if (!speaker.Configure("127.0.0.1", voicePort, speakerId) || !speaker.Start())
        return Fail("speaker client start");
    if (!listener.Configure("127.0.0.1", voicePort, listenerId) || !listener.Start())
        return Fail("listener client start");
    if (!WaitFor([&] { return speaker.IsConnected() && listener.IsConnected(); }, std::chrono::seconds(3)))
        return Fail("voice client handshakes");

    ov::client::OVBassApi bass;
    ov::client::OVAudioEngine audio(bass);
    if (!audio.Initialize(0)) return Fail("BASS no-sound output initialization");

    std::atomic_uint32_t sequence{};
    std::atomic_uint64_t sentFrames{};
    std::atomic_uint64_t receivedFrames{};
    listener.SetFrameHandler([&](ov::VoiceFrame frame) {
        ++receivedFrames;
        audio.OnRemoteFrame(std::move(frame));
    });

    if (mode == "ptt")
    {
        audio.EnableNoiseSuppression(false);
        audio.EnableAGC(false);
        audio.SetTransmitting(true);
        audio.SetFrameHandler([&](std::vector<std::uint8_t> encoded) {
            ov::VoiceFrame frame;
            frame.channelId = 0;
            frame.sequence = static_cast<std::uint16_t>(++sequence);
            frame.encoded = std::move(encoded);
            if (speaker.SendVoiceFrame(std::move(frame))) ++sentFrames;
        });
        if (!audio.StartCapture()) return Fail("microphone capture start");
        if (!listener.SendVoiceBegin(0) || !speaker.SendVoiceBegin(0)) return Fail("voice begin");
        if (!WaitFor([&] { return sentFrames.load() >= 10 && receivedFrames.load() >= 3 && audio.PlayedSamples() > 0; },
                     std::chrono::seconds(5)))
            return Fail("full voice path warmup");
    }

    Snapshot baseline;
    if (!ReadSnapshot(baseline)) return Fail("baseline process snapshot");
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(durationSeconds);
    auto nextProgress = start + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (!speaker.IsConnected() || !listener.IsConnected()) return Fail("network disconnected during soak");
        if (mode == "ptt" && !audio.IsCapturing()) return Fail("capture stopped during PTT soak");
        const auto now = std::chrono::steady_clock::now();
        if (now >= nextProgress)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            std::cout << "progress=" << elapsed << "s sent=" << sentFrames.load()
                      << " received=" << receivedFrames.load() << " played=" << audio.PlayedSamples() << std::endl;
            nextProgress += std::chrono::seconds(30);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    Snapshot final;
    if (!ReadSnapshot(final)) return Fail("final process snapshot");
    if (final.handles > baseline.handles + 8) return Fail("handle growth during soak");
    if (final.privateBytes > baseline.privateBytes + 8U * 1024U * 1024U) return Fail("private memory growth during soak");
    if (mode == "ptt" && (sentFrames.load() < static_cast<std::uint64_t>(durationSeconds) * 10U ||
                           receivedFrames.load() < static_cast<std::uint64_t>(durationSeconds) * 10U))
        return Fail("insufficient sustained voice traffic");

    audio.SetTransmitting(false);
    if (mode == "ptt")
    {
        speaker.SendVoiceEnd();
        listener.SendVoiceEnd();
    }
    audio.Shutdown();
    listener.Stop();
    speaker.Stop();
    server.Stop();
    std::cout << "PASS mode=" << mode << " duration=" << durationSeconds << "s port=" << voicePort
              << " sent=" << sentFrames.load()
              << " received=" << receivedFrames.load() << " handles=" << baseline.handles << "->" << final.handles
              << " private_bytes=" << baseline.privateBytes << "->" << final.privateBytes << '\n';
    return 0;
}
