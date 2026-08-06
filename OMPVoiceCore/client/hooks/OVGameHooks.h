#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

namespace ov::client
{
class OVDx9Renderer;
class OVGameHooks final
{
public:
    bool Initialize();
    bool InstallDx9Hooks(const std::shared_ptr<OVDx9Renderer>& renderer);
    void Shutdown();
    [[nodiscard]] bool IsSupportedClient() const noexcept { return supported_; }
    [[nodiscard]] bool IsKeyDown(int virtualKey) const noexcept;
    [[nodiscard]] bool IsSettingsPressed() const noexcept { return settingsPressed_; }
    [[nodiscard]] bool IsDebugPressed() const noexcept { return debugPressed_; }
    void PollHotkeys();
    [[nodiscard]] bool LoopbackToggled() const noexcept { return loopbackToggled_; }
    [[nodiscard]] bool FakeRemoteToggled() const noexcept { return fakeRemoteToggled_; }
    [[nodiscard]] bool OverlayToggled() const noexcept { return overlayToggled_; }
    void ClearEdgeEvents() noexcept { settingsPressed_ = false; debugPressed_ = false; }

private:
    bool supported_{};
    bool dx9Hooked_{};
    bool settingsPressed_{};
    bool debugPressed_{};
    bool loopbackToggled_{};
    bool fakeRemoteToggled_{};
    bool overlayToggled_{};
    std::uint16_t previousKeys_{};
};
}
