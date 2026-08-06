#pragma once

#include "shared/OVPacket.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ov::client
{
class OVNetworkClient final
{
public:
    using FrameHandler = std::function<void(VoiceFrame)>;
    OVNetworkClient();
    ~OVNetworkClient();
    bool Configure(std::string host, std::uint16_t port, std::uint16_t playerId);
    bool Start();
    void Stop();
    void SetFrameHandler(FrameHandler handler);
    bool SendVoiceBegin(std::uint32_t channelId);
    bool SendVoiceFrame(VoiceFrame frame);
    bool SendVoiceEnd();
    [[nodiscard]] bool IsConnected() const noexcept { return connected_.load(); }
    [[nodiscard]] std::uint32_t SentPackets() const noexcept { return sentPackets_.load(); }
    [[nodiscard]] std::uint32_t ReceivedPackets() const noexcept { return receivedPackets_.load(); }

private:
    bool OpenSocket();
    void CloseSocket();
    bool Send(const std::vector<std::uint8_t>& packet);
    void ReceiveLoop();
    void TryReconnect();
    std::string host_{"127.0.0.1"};
    std::uint16_t port_{7775};
    std::uint16_t playerId_{};
    std::intptr_t socket_{-1};
    std::atomic_bool running_{};
    std::atomic_bool connected_{};
    std::thread thread_;
    std::mutex socketMutex_;
    std::mutex callbackMutex_;
    FrameHandler frameHandler_;
    std::chrono::steady_clock::time_point nextRetry_{};
    std::size_t retryIndex_{};
    std::atomic_uint32_t sentPackets_{};
    std::atomic_uint32_t receivedPackets_{};
};
}
