#include "OVGameHooks.h"

#include "shared/OVLogger.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace ov::client
{
bool OVGameHooks::Initialize()
{
#ifdef _WIN32
    supported_ = GetModuleHandleA("samp.dll") != nullptr || GetModuleHandleA("open.mp.dll") != nullptr;
#else
    supported_ = false;
#endif
    if (!supported_) OV_LOG_WARN("Hooks", "SA:MP/open.mp module not detected; waiting for game module");
    return true;
}
void OVGameHooks::Shutdown() { supported_ = false; }
bool OVGameHooks::IsKeyDown(int virtualKey) const noexcept
{
#ifdef _WIN32
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
#else
    (void)virtualKey; return false;
#endif
}
void OVGameHooks::PollHotkeys()
{
    const std::uint16_t keys = static_cast<std::uint16_t>((IsKeyDown(VK_F11) ? 1 : 0) | (IsKeyDown(VK_F10) ? 2 : 0) | (IsKeyDown(VK_F8) ? 4 : 0) | (IsKeyDown(VK_F9) ? 8 : 0));
    settingsPressed_ = (keys & 1U) != 0 && (previousKeys_ & 1U) == 0;
    debugPressed_ = (keys & 2U) != 0 && (previousKeys_ & 2U) == 0;
    if ((keys & 4U) != 0 && (previousKeys_ & 4U) == 0) loopbackToggled_ = !loopbackToggled_;
    if ((keys & 8U) != 0 && (previousKeys_ & 8U) == 0) fakeRemoteToggled_ = !fakeRemoteToggled_;
    if (debugPressed_) overlayToggled_ = !overlayToggled_;
    previousKeys_ = keys;
}
}
