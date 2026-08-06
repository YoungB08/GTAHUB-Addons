#pragma once

#include "OVUdpServer.h"
#include "server/channels/OVChannelManager.h"
#include "shared/OVPacket.h"

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ov::server
{
class OVVoiceServer final
{
public:
    explicit OVVoiceServer(OVChannelManager& channels);
    ~OVVoiceServer();
    bool Start();
    bool Start(std::uint16_t port);
    void Stop();
    void UpdatePlayer(int playerId, const Vector3& position, int vehicleId);
    void RemovePlayer(int playerId);

private:
    struct Session
    {
        UdpEndpoint endpoint;
        int playerId{-1};
        std::chrono::steady_clock::time_point windowStart{};
        std::uint32_t packetsInWindow{};
        bool handshaken{};
    };

    void OnPacket(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& datagram);
    void OnHandshake(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& datagram);
    void OnControl(const UdpEndpoint& endpoint, OVPacket type, const std::vector<std::uint8_t>& payload);
    void OnVoiceData(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& datagram);
    bool RateLimit(Session& session);
    void SendToPlayer(int playerId, const std::vector<std::uint8_t>& packet);

    OVChannelManager& channels_;
    OVUdpServer socket_;
    std::mutex mutex_;
    std::unordered_map<std::string, Session> sessions_;
    std::unordered_map<int, std::string> playerEndpoints_;
};
}
