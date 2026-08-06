#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace ov::server
{
struct UdpEndpoint
{
    std::string address;
    std::uint16_t port{};
    [[nodiscard]] std::string Key() const { return address + ":" + std::to_string(port); }
    bool operator==(const UdpEndpoint& other) const { return address == other.address && port == other.port; }
};

class OVUdpServer final
{
public:
    using PacketHandler = std::function<void(const UdpEndpoint&, const std::vector<std::uint8_t>&)>;

    OVUdpServer();
    ~OVUdpServer();
    bool Start(std::uint16_t port, PacketHandler handler);
    void Stop();
    bool Send(const UdpEndpoint& endpoint, const std::vector<std::uint8_t>& packet);
    [[nodiscard]] bool IsRunning() const noexcept { return running_.load(); }

private:
    void ReceiveLoop();
    std::intptr_t socket_{-1};
    std::atomic_bool running_{};
    std::thread thread_;
    PacketHandler handler_;
};
}
