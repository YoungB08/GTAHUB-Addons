#pragma once

#include <cstdint>

namespace Offsets {
    constexpr uintptr_t CChat_RenderEntry    = 0x66EE0;
    constexpr uintptr_t CChat_AddChatMessage = 0x67A90;
    constexpr uintptr_t CChat_AddMessage     = 0x67BE0;
    constexpr uintptr_t CInput_Send          = 0x69340;
    constexpr uintptr_t CInput_ProcessInput   = 0x69410;
    constexpr uintptr_t CInput_Open          = 0x68EC0;
    constexpr uintptr_t CInput_Close         = 0x68FC0;
}

namespace HookManager {
    bool Install();
    void Uninstall();

    bool InstallChatHooks();
    bool InstallInputHooks();
    bool InstallD3DHooks();
}
