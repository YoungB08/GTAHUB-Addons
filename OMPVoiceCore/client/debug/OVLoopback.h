#pragma once

namespace ov::client
{
class OVLoopback final
{
public:
    void SetEnabled(bool enabled) noexcept { enabled_ = enabled; }
    void Toggle() noexcept { enabled_ = !enabled_; }
    [[nodiscard]] bool Enabled() const noexcept { return enabled_; }

private:
    bool enabled_{};
};
}
