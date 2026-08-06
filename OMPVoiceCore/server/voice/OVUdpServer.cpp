#include "OVUdpServer.h"

#include "shared/OVLogger.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using ov_socket_t = SOCKET;
static constexpr ov_socket_t kInvalidSocket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using ov_socket_t = int;
static constexpr ov_socket_t kInvalidSocket = -1;
#endif

#include <array>
#include <cstring>

namespace ov::server
{
namespace
{
void CloseSocket(ov_socket_t socket)
{
#ifdef _WIN32
    if (socket != kInvalidSocket) closesocket(socket);
#else
    if (socket != kInvalidSocket) close(socket);
#endif
}

bool AddressFrom(const sockaddr_storage& storage, UdpEndpoint& endpoint)
{
    char address[INET6_ADDRSTRLEN]{};
    const void* raw = nullptr;
    if (storage.ss_family == AF_INET)
    {
        raw = &reinterpret_cast<const sockaddr_in&>(storage).sin_addr;
        endpoint.port = ntohs(reinterpret_cast<const sockaddr_in&>(storage).sin_port);
    }
    else if (storage.ss_family == AF_INET6)
    {
        raw = &reinterpret_cast<const sockaddr_in6&>(storage).sin6_addr;
        endpoint.port = ntohs(reinterpret_cast<const sockaddr_in6&>(storage).sin6_port);
    }
    else return false;
    if (inet_ntop(storage.ss_family, raw, address, sizeof(address)) == nullptr) return false;
    endpoint.address = address;
    return true;
}

bool StorageFrom(const UdpEndpoint& endpoint, sockaddr_storage& storage, int& length)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    if (inet_pton(AF_INET, endpoint.address.c_str(), &address.sin_addr) != 1) return false;
    std::memcpy(&storage, &address, sizeof(address));
    length = sizeof(address);
    return true;
}
}

OVUdpServer::OVUdpServer() = default;
OVUdpServer::~OVUdpServer() { Stop(); }

bool OVUdpServer::Start(std::uint16_t port, PacketHandler handler)
{
    if (running_) return false;
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0)
    {
        OV_LOG_ERROR("Network", "WSAStartup failed");
        return false;
    }
#endif
    const auto socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket == kInvalidSocket)
    {
        OV_LOG_ERROR("Network", "UDP socket creation failed");
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
#ifdef _WIN32
    BOOL exclusive = TRUE;
    if (setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                   reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0)
    {
        OV_LOG_ERROR("Network", "UDP exclusive-address setup failed");
        CloseSocket(socket);
        WSACleanup();
        return false;
    }
#else
    int reuse = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
    {
        OV_LOG_ERROR("Network", "UDP bind port %u failed", port);
        CloseSocket(socket);
#ifdef _WIN32
        WSACleanup();
#endif
        return false;
    }
#ifdef _WIN32
    DWORD timeout = 1000;
#else
    timeval timeout{1, 0};
#endif
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    socket_ = static_cast<std::intptr_t>(socket);
    handler_ = std::move(handler);
    running_ = true;
    thread_ = std::thread(&OVUdpServer::ReceiveLoop, this);
    OV_LOG_INFO("Network", "Voice UDP bound to %u", port);
    return true;
}

void OVUdpServer::Stop()
{
    if (!running_.exchange(false)) return;
    const auto socket = static_cast<ov_socket_t>(socket_);
    socket_ = -1;
    CloseSocket(socket);
    if (thread_.joinable()) thread_.join();
#ifdef _WIN32
    WSACleanup();
#endif
    OV_LOG_INFO("Network", "Voice UDP stopped");
}

bool OVUdpServer::Send(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& packet)
{
    if (!running_ || packet.empty()) return false;
    sockaddr_storage storage{};
    int length = 0;
    if (!StorageFrom(endpoint, storage, length)) return false;
    const auto result = sendto(static_cast<ov_socket_t>(socket_), reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0, reinterpret_cast<const sockaddr*>(&storage), length);
    return result == static_cast<int>(packet.size());
}

void OVUdpServer::ReceiveLoop()
{
    std::array<std::uint8_t, 2048> buffer{};
    while (running_)
    {
        sockaddr_storage source{};
#ifdef _WIN32
        int sourceLength = sizeof(source);
#else
        socklen_t sourceLength = sizeof(source);
#endif
        const auto received = recvfrom(static_cast<ov_socket_t>(socket_), reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0, reinterpret_cast<sockaddr*>(&source), &sourceLength);
        if (received <= 0) continue;
        UdpEndpoint endpoint;
        if (!AddressFrom(source, endpoint)) continue;
        handler_(endpoint, std::vector<std::uint8_t>(buffer.begin(), buffer.begin() + received));
    }
}
}
