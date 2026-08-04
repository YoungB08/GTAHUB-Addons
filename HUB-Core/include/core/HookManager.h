#pragma once

#include <cstdint>

namespace Offsets {
constexpr uintptr_t CInput_Open = 0x68EC0;
constexpr uintptr_t CInput_Close = 0x68FC0;
}

namespace HookManager {

bool Install();
void Uninstall();
bool IsInstalled();

bool InstallChatHooks();
bool InstallInputHooks();
bool InstallD3DHooks();

bool SendChatText(const char* text);
void CloseChatInput();
bool SetNativeChatSuppressed(bool suppressed);
void SyncSampChat();

} // namespace HookManager
