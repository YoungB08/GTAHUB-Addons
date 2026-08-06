#include "OVNetworkClient.h"

#include "shared/OVConstants.h"
#include "shared/OVLogger.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
using ov_client_socket_t = SOCKET;
static constexpr ov_client_socket_t kInvalidClientSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using ov_client_socket_t = int;
static constexpr ov_client_socket_t kInvalidClientSocket = -1;
#endif

#include <array>

namespace ov::client
{
OVNetworkClient::OVNetworkClient() = default;
OVNetworkClient::~OVNetworkClient() { Stop(); }
bool OVNetworkClient::Configure(std::string host, std::uint16_t port, std::uint16_t playerId)
{
    if (host.empty() || port == 0) return false;
    host_ = std::move(host); port_ = port; playerId_ = playerId; return true;
}
bool OVNetworkClient::OpenSocket()
{
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return false;
#endif
    const auto socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == kInvalidClientSocket) return false;
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &endpoint.sin_addr) != 1)
    {
#ifdef _WIN32
        closesocket(socket);
        WSACleanup();
#else
        close(socket);
#endif
        return false;
    }
    socket_ = static_cast<std::intptr_t>(socket);
    connected_ = false;
    return true;
}
void OVNetworkClient::CloseSocket()
{
    const auto socket = static_cast<ov_client_socket_t>(socket_);
    socket_ = -1;
#ifdef _WIN32
    if (socket != kInvalidClientSocket) closesocket(socket);
    WSACleanup();
#else
    if (socket != kInvalidClientSocket) close(socket);
#endif
    connected_ = false;
}
bool OVNetworkClient::Start()
{
    if (running_ || !OpenSocket()) return false;
    running_ = true;
    retryIndex_ = 0;
    nextRetry_ = std::chrono::steady_clock::now();
    lastReceive_ = std::chrono::steady_clock::now();
    nextPing_ = lastReceive_ + std::chrono::seconds(2);
    thread_ = std::thread(&OVNetworkClient::ReceiveLoop, this);
    return true;
}
void OVNetworkClient::Stop()
{
    if (!running_.exchange(false)) return;
    { std::lock_guard<std::mutex> lock(socketMutex_); CloseSocket(); }
    if (thread_.joinable()) thread_.join();
}
void OVNetworkClient::SetFrameHandler(FrameHandler handler) { std::lock_guard<std::mutex> lock(callbackMutex_); frameHandler_ = std::move(handler); }

bool OVNetworkClient::Send(const std::vector<std::uint8_t>& packet)
{
    if (!running_ || packet.empty()) return false;
    sockaddr_in endpoint{};
    endpoint.sin_family = AF_INET;
    endpoint.sin_port = htons(port_);
    if (inet_pton(AF_INET, host_.c_str(), &endpoint.sin_addr) != 1) return false;
    std::lock_guard<std::mutex> lock(socketMutex_);
    if (socket_ == -1) return false;
    const auto result = sendto(static_cast<ov_client_socket_t>(socket_), reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
    if (result == static_cast<int>(packet.size())) { ++sentPackets_; sentBytes_ += packet.size(); return true; }
    connected_ = false;
    return false;
}
bool OVNetworkClient::SendVoiceBegin(std::uint32_t channelId)
{
    (void)channelId;
    return connected_ && Send(SerializeControl(OVPacket::VoiceBegin, playerId_));
}
bool OVNetworkClient::SendVoiceFrame(VoiceFrame frame)
{
    frame.playerId = playerId_;
    return connected_ && Send(SerializeVoiceFrame(frame));
}
bool OVNetworkClient::SendVoiceEnd() { return connected_ && Send(SerializeControl(OVPacket::VoiceEnd, playerId_)); }

void OVNetworkClient::TryReconnect()
{
    if (connected_ || std::chrono::steady_clock::now() < nextRetry_) return;
    const auto handshake = SerializeHandshake(Handshake{});
    if (Send(handshake))
    {
        OV_LOG_INFO("Network", "Handshake sent to %s:%u", host_.c_str(), port_);
    }
    const std::array<int, 4> retrySeconds{1, 3, 5, 10};
    nextRetry_ = std::chrono::steady_clock::now() + std::chrono::seconds(retrySeconds[std::min(retryIndex_, retrySeconds.size() - 1)]);
    retryIndex_ = std::min(retryIndex_ + 1, retrySeconds.size() - 1);
}
void OVNetworkClient::ReceiveLoop()
{
    std::array<std::uint8_t, 2048> buffer{};
    while (running_)
    {
        const auto now = std::chrono::steady_clock::now();
        if (connected_ && now - lastReceive_ > std::chrono::seconds(5))
        {
            connected_ = false;
            retryIndex_ = 0;
            nextRetry_ = now;
            OV_LOG_WARN("Network", "Voice server timed out; reconnecting");
        }
        if (connected_ && now >= nextPing_)
        {
            Send(SerializeControl(OVPacket::Ping));
            nextPing_ = now + std::chrono::seconds(2);
        }
        TryReconnect();
#ifdef _WIN32
        fd_set readSet; FD_ZERO(&readSet); FD_SET(static_cast<SOCKET>(socket_), &readSet);
        timeval timeout{0, 100000};
        if (select(0, &readSet, nullptr, nullptr, &timeout) <= 0) continue;
#else
        fd_set readSet; FD_ZERO(&readSet); FD_SET(static_cast<int>(socket_), &readSet);
        timeval timeout{0, 100000};
        if (select(static_cast<int>(socket_) + 1, &readSet, nullptr, nullptr, &timeout) <= 0) continue;
#endif
        const auto received = recvfrom(static_cast<ov_client_socket_t>(socket_), reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0, nullptr, nullptr);
        if (received <= 0) { connected_ = false; continue; }
        ++receivedPackets_;
        const auto packet = DecodeDatagram(buffer.data(), static_cast<std::size_t>(received));
        if (!packet) continue;
        lastReceive_ = std::chrono::steady_clock::now();
        if (packet->type == OVPacket::Pong)
        {
            connected_ = true;
            retryIndex_ = 0;
            nextPing_ = lastReceive_ + std::chrono::seconds(2);
            continue;
        }
        if (packet->type != OVPacket::VoiceData) continue;
        const auto frame = ParseVoiceFrame(std::vector<std::uint8_t>(buffer.begin(), buffer.begin() + received));
        if (!frame) continue;
        FrameHandler callback;
        { std::lock_guard<std::mutex> lock(callbackMutex_); callback = frameHandler_; }
        if (callback) callback(*frame);
    }
}
}
