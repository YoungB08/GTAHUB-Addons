#pragma once

#include <cstdint>
#include <memory>
#include <vector>

struct OpusDecoder;
namespace ov::client
{
class OVOpusDecoder final
{
public:
    OVOpusDecoder();
    ~OVOpusDecoder();
    bool Initialize();
    void Reset();
    std::vector<std::int16_t> Decode(const std::uint8_t* encoded, std::size_t size, bool plc);
    [[nodiscard]] bool IsReady() const noexcept { return decoder_ != nullptr; }

private:
    OpusDecoder* decoder_{};
    std::vector<std::int16_t> pcm_;
};
}
