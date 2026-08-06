#include "OVVoiceServer.h"

#include "shared/OVConstants.h"
#include "shared/OVLogger.h"
#include "shared/OVPacket.h"

#include <algorithm>
#include <cstring>

namespace ov::server
{
namespace
{
bool ReadU32(const std::vector<std::uint8_t>& payload, std::uint32_t& value)
{
    if (payload.size() != sizeof(value)) return false;
    value = static_cast<std::uint32_t>(payload[0]) |
        (static_cast<std::uint32_t>(payload[1]) << 8U) |
        (static_cast<std::uint32_t>(payload[2]) << 16U) |
        (static_cast<std::uint32_t>(payload[3]) << 24U);
    return true;
}
}

OVVoiceServer::OVVoiceServer(OVChannelManager& channels) : channels_(channels) {}
OVVoiceServer::~OVVoiceServer() { Stop(); }

bool OVVoiceServer::Start()
{
    return Start(OMPVOICE_PORT);
}

bool OVVoiceServer::Start(std::uint16_t port)
{
    return socket_.Start(port, [this](const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& datagram) { OnPacket(endpoint, datagram); });
}

void OVVoiceServer::Stop()
{
    socket_.Stop();
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
    playerEndpoints_.clear();
}

void OVVoiceServer::UpdatePlayer(int playerId, const Vector3& position, int vehicleId) { channels_.UpsertPlayer(playerId, position, vehicleId); }
void OVVoiceServer::RemovePlayer(int playerId)
{
    channels_.RemovePlayer(playerId);
    std::lock_guard<std::mutex> lock(mutex_);
    const auto endpoint = playerEndpoints_.find(playerId);
    if (endpoint != playerEndpoints_.end()) { sessions_.erase(endpoint->second); playerEndpoints_.erase(endpoint); }
}

bool OVVoiceServer::RateLimit(Session& session)
{
    const auto now = std::chrono::steady_clock::now();
    if (session.windowStart.time_since_epoch().count() == 0 || now - session.windowStart >= std::chrono::seconds(1))
    {
        session.windowStart = now;
        session.packetsInWindow = 0;
    }
    return ++session.packetsInWindow <= 120;
}

void OVVoiceServer::OnPacket(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& datagram)
{
    if (datagram.size() > 1400) return;
    const auto packet = DecodeDatagram(datagram.data(), datagram.size());
    if (!packet) return;
    if (packet->type == OVPacket::Handshake) { OnHandshake(endpoint, datagram); return; }
    if (packet->type == OVPacket::VoiceData) { OnVoiceData(endpoint, datagram); return; }
    OnControl(endpoint, packet->type, packet->payload);
}

void OVVoiceServer::OnHandshake(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& datagram)
{
    const auto handshake = ParseHandshake(datagram);
    if (!handshake || handshake->magic != OMPVOICE_MAGIC || handshake->version != OMPVOICE_PROTOCOL_VERSION || handshake->uid != OMPVOICE_UID)
    {
        OV_LOG_WARN("Security", "Rejected invalid voice handshake from %s", endpoint.Key().c_str());
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto& session = sessions_[endpoint.Key()];
    session.endpoint = endpoint;
    session.handshaken = true;
    session.windowStart = std::chrono::steady_clock::now();
    session.packetsInWindow = 0;
    socket_.Send(endpoint, SerializeControl(OVPacket::Pong, OMPVOICE_PROTOCOL_VERSION));
    OV_LOG_DEBUG("Network", "Handshake accepted from %s", endpoint.Key().c_str());
}

void OVVoiceServer::OnControl(const UdpEndpoint& endpoint, OVPacket type, const std::vector<std::uint8_t>& payload)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto sessionIt = sessions_.find(endpoint.Key());
    if (sessionIt == sessions_.end() || !sessionIt->second.handshaken || !RateLimit(sessionIt->second)) return;
    auto& session = sessionIt->second;
    if (type == OVPacket::Ping)
    {
        socket_.Send(endpoint, SerializeControl(OVPacket::Pong));
        return;
    }
    std::uint32_t playerValue = 0;
    if ((type != OVPacket::VoiceBegin && type != OVPacket::VoiceEnd) || !ReadU32(payload, playerValue) || playerValue > 1000) return;
    const int playerId = static_cast<int>(playerValue);
    if (type == OVPacket::VoiceBegin)
    {
        if (session.playerId != -1 && session.playerId != playerId) return;
        session.playerId = playerId;
        playerEndpoints_[playerId] = endpoint.Key();
        channels_.SetTalking(playerId, true);
    }
    else if (session.playerId != playerId) return;
    if (type == OVPacket::VoiceEnd) channels_.SetTalking(playerId, false);
    const auto recipients = channels_.Recipients(playerId, 0);
    const auto outgoing = SerializeControl(type, static_cast<std::uint32_t>(playerId));
    for (const auto& recipient : recipients) SendToPlayer(recipient.playerId, outgoing);
}

void OVVoiceServer::OnVoiceData(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& datagram)
{
    const auto frame = ParseVoiceFrame(datagram);
    if (!frame) return;
    std::lock_guard<std::mutex> lock(mutex_);
    const auto sessionIt = sessions_.find(endpoint.Key());
    if (sessionIt == sessions_.end() || !sessionIt->second.handshaken || !RateLimit(sessionIt->second)) return;
    const auto& session = sessionIt->second;
    if (session.playerId < 0 || session.playerId != frame->playerId || !channels_.IsEnabled(session.playerId)) return;
    for (const auto& recipient : channels_.Recipients(frame->playerId, frame->channelId))
    {
        VoiceFrame outgoing = *frame;
        outgoing.gain = recipient.gain;
        outgoing.pan = recipient.pan;
        outgoing.mode = recipient.mode;
        SendToPlayer(recipient.playerId, SerializeVoiceFrame(outgoing));
    }
}

void OVVoiceServer::SendToPlayer(int playerId, const std::vector<std::uint8_t>& packet)
{
    const auto endpoint = playerEndpoints_.find(playerId);
    if (endpoint == playerEndpoints_.end()) return;
    const auto session = sessions_.find(endpoint->second);
    if (session != sessions_.end()) socket_.Send(session->second.endpoint, packet);
}
}
