#include "client/network/OVNetworkClient.h"
#include "server/channels/OVChannelManager.h"
#include "server/voice/OVVoiceServer.h"
#include "shared/OVConstants.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <atomic>
#include <memory>
#include <thread>
#include <vector>

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
    static_assert(ov::OMPVOICE_UID == 0xD6FEE4A6B0EA27A3ULL);
    ov::server::OVChannelManager channels;
    ov::server::OVVoiceServer server(channels);
    if (!server.Start()) return Fail("UDP 7775 bind");

    std::vector<std::unique_ptr<ov::client::OVNetworkClient>> clients;
    for (std::uint16_t playerId = 1; playerId <= 50; ++playerId)
    {
        channels.UpsertPlayer(playerId, {static_cast<float>(playerId), 0.0F, 0.0F}, -1);
        auto client = std::make_unique<ov::client::OVNetworkClient>();
        if (!client->Configure("127.0.0.1", ov::OMPVOICE_PORT, playerId) || !client->Start()) return Fail("fake client start");
        clients.push_back(std::move(client));
    }
    if (!WaitFor([&clients] {
            for (const auto& client : clients) if (!client->IsConnected()) return false;
            return true;
        }, std::chrono::seconds(5))) return Fail("50-client handshake");
    std::atomic_bool routedVoice{};
    clients[1]->SetFrameHandler([&routedVoice](ov::VoiceFrame frame) { routedVoice = frame.playerId == 1; });
    if (!clients[1]->SendVoiceBegin(2)) return Fail("listener voice begin");
    if (!clients[0]->SendVoiceBegin(1)) return Fail("voice begin");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ov::VoiceFrame voice;
    voice.channelId = 0;
    voice.sequence = 1;
    voice.encoded = {1, 2, 3, 4};
    if (!clients[0]->SendVoiceFrame(std::move(voice))) return Fail("voice data");
    if (!WaitFor([&routedVoice] { return routedVoice.load(); }, std::chrono::seconds(2))) return Fail("voice routing");
    clients[0]->SendVoiceEnd();
    const auto bytesBefore = clients[0]->SentBytes();
    const auto latencyStart = std::chrono::steady_clock::now();
    std::atomic_int latencyMs{-1};
    clients[1]->SetFrameHandler([&latencyMs, latencyStart](ov::VoiceFrame frame) {
        if (frame.sequence == 2) latencyMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - latencyStart).count());
    });
    clients[0]->SendVoiceBegin(1);
    for (std::uint16_t sequence = 2; sequence < 52; ++sequence)
    {
        ov::VoiceFrame measured;
        measured.channelId = 0;
        measured.sequence = sequence;
        measured.encoded.assign(100, 0x55);
        if (!clients[0]->SendVoiceFrame(std::move(measured))) return Fail("measured voice data");
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    clients[0]->SendVoiceEnd();
    if (!WaitFor([&latencyMs] { return latencyMs.load() >= 0; }, std::chrono::seconds(2))) return Fail("voice latency measurement");
    if (latencyMs.load() >= 80) return Fail("voice latency target");
    const auto bytesPerSecond = clients[0]->SentBytes() - bytesBefore;
    if (bytesPerSecond >= 8192) return Fail("voice bandwidth target");
    for (auto& client : clients) client->Stop();
    clients.clear();

    for (int iteration = 0; iteration < 100; ++iteration)
    {
        ov::client::OVNetworkClient client;
        if (!client.Configure("127.0.0.1", ov::OMPVOICE_PORT, 1) || !client.Start()) return Fail("connect cycle start");
        if (!WaitFor([&client] { return client.IsConnected(); }, std::chrono::seconds(1))) return Fail("connect cycle handshake");
        client.Stop();
    }

    ov::client::OVNetworkClient reconnecting;
    if (!reconnecting.Configure("127.0.0.1", ov::OMPVOICE_PORT, 1) || !reconnecting.Start()) return Fail("reconnect client start");
    if (!WaitFor([&reconnecting] { return reconnecting.IsConnected(); }, std::chrono::seconds(2))) return Fail("initial reconnect handshake");
    server.Stop();
    if (!WaitFor([&reconnecting] { return !reconnecting.IsConnected(); }, std::chrono::seconds(7))) return Fail("disconnect timeout");
    if (!server.Start()) return Fail("server restart");
    if (!WaitFor([&reconnecting] { return reconnecting.IsConnected(); }, std::chrono::seconds(12))) return Fail("automatic reconnect");
    reconnecting.Stop();
    server.Stop();

    std::cout << "UDP bind, handshake, client load, and reconnect tests passed\n";
    return 0;
}
