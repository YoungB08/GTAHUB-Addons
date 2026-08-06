#pragma once

#include "shared/OVTypes.h"

#include <cstdint>

namespace ov::client
{
struct FakeRemotePlayer { std::uint16_t id{999}; Vector3 position{5.0F, 0.0F, 0.0F}; bool talking{}; float elapsed{}; };
class OVFakeRemote final
{
public:
    void SetEnabled(bool enabled) noexcept { enabled_ = enabled; }
    void Toggle() noexcept { enabled_ = !enabled_; }
    void Update(float deltaSeconds);
    [[nodiscard]] bool Enabled() const noexcept { return enabled_; }
    [[nodiscard]] const FakeRemotePlayer& Player() const noexcept { return player_; }

private:
    bool enabled_{};
    FakeRemotePlayer player_;
};
}
