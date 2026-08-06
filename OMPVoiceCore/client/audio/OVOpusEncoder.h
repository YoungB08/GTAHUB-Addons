#pragma once

#include <cstdint>
#include <memory>
#include <vector>

struct OpusEncoder;
namespace ov::client
{
class OVOpusEncoder final
{
public:
    OVOpusEncoder();
    ~OVOpusEncoder();
    bool Initialize();
    void Shutdown();
    std::vector<std::uint8_t> Encode(const std::int16_t* pcm, std::size_t samples);
    [[nodiscard]] bool IsReady() const noexcept { return encoder_ != nullptr; }

private:
    OpusEncoder* encoder_{};
    std::vector<std::uint8_t> buffer_;
};
}
