#pragma once

#include <cmath>
#include <cstdint>

namespace ov
{
struct Vector3
{
    float x{};
    float y{};
    float z{};

    [[nodiscard]] float DistanceTo(const Vector3& other) const noexcept
    {
        const float dx = x - other.x;
        const float dy = y - other.y;
        const float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }
};

enum class VoiceMode : std::uint8_t
{
    Proximity,
    Vehicle,
    Radio,
    Phone,
    Global
};

struct VoiceChannel
{
    std::uint32_t id{};
    float hearDistance{25.0F};
    bool positional{true};
    VoiceMode mode{VoiceMode::Proximity};
};
}
