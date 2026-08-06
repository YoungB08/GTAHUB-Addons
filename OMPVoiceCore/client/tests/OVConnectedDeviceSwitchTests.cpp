#include "audio/OVAudioEngine.h"
#include "audio/OVBassApi.h"
#include "network/OVNetworkClient.h"
#include "server/channels/OVChannelManager.h"
#include "server/voice/OVVoiceServer.h"
#include "shared/OVConstants.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <thread>

namespace
{
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

int Fail(const char* message)
{
    std::cerr << "FAIL: " << message << '\n';
    return 1;
}
}

int main()
{
    constexpr std::uint16_t playerId = 7;
    ov::server::OVChannelManager channels;
    channels.UpsertPlayer(playerId, {}, -1);
    ov::server::OVVoiceServer server(channels);
    if (!server.Start()) return Fail("UDP voice server start");

    ov::client::OVNetworkClient network;
    if (!network.Configure("127.0.0.1", ov::OMPVOICE_PORT, playerId) || !network.Start())
        return Fail("voice client start");
    if (!WaitFor([&network] { return network.IsConnected(); }, std::chrono::seconds(3)))
        return Fail("voice client handshake");

    ov::client::OVBassApi bass;
    ov::client::OVAudioEngine audio(bass);
    if (!audio.Initialize(0)) return Fail("BASS no-sound output initialization");
    const auto devices = bass.RecordDevices();
    if (devices.empty()) return Fail("no recording device available");

    std::atomic_uint32_t sequence{};
    std::atomic_uint32_t encodedFrames{};
    audio.EnableNoiseSuppression(false);
    audio.EnableAGC(false);
    audio.SetTransmitting(true);
    audio.SetFrameHandler([&](std::vector<std::uint8_t> encoded) {
        ov::VoiceFrame frame;
        frame.channelId = 0;
        frame.sequence = static_cast<std::uint16_t>(++sequence);
        frame.encoded = std::move(encoded);
        if (network.SendVoiceFrame(std::move(frame))) ++encodedFrames;
    });
    if (!audio.StartCapture()) return Fail("default microphone capture start");
    if (!network.SendVoiceBegin(0)) return Fail("voice begin before device switch");
    if (!WaitFor([&encodedFrames] { return encodedFrames.load() >= 3; }, std::chrono::seconds(3)))
        return Fail("encoded voice before device switch");

    int selectedDevice = -1;
    for (std::size_t index = 0; index < devices.size(); ++index)
    {
        if (audio.SetInputDevice(static_cast<int>(index)))
        {
            selectedDevice = static_cast<int>(index);
            break;
        }
    }
    if (selectedDevice < 0 || audio.InputDevice() != selectedDevice || !audio.IsCapturing())
        return Fail("live microphone device switch");

    const auto framesAfterSwitch = encodedFrames.load();
    const auto receivesAfterSwitch = network.ReceivedPackets();
    if (!WaitFor([&] {
            return network.IsConnected() && encodedFrames.load() >= framesAfterSwitch + 3 &&
                network.ReceivedPackets() > receivesAfterSwitch;
        }, std::chrono::seconds(4)))
        return Fail("capture or network heartbeat after device switch");

    const int invalidDevice = static_cast<int>(devices.size()) + 64;
    if (audio.SetInputDevice(invalidDevice)) return Fail("invalid microphone device accepted");
    if (audio.InputDevice() != selectedDevice || !audio.IsCapturing())
        return Fail("previous microphone was not restored");
    const auto framesAfterRollback = encodedFrames.load();
    if (!WaitFor([&] { return network.IsConnected() && encodedFrames.load() >= framesAfterRollback + 3; },
                 std::chrono::seconds(3)))
        return Fail("capture or connection after microphone rollback");

    audio.SetTransmitting(false);
    network.SendVoiceEnd();
    audio.Shutdown();
    network.Stop();
    server.Stop();
    std::cout << "Connected microphone switch and rollback passed using " << devices[selectedDevice] << '\n';
    return 0;
}
