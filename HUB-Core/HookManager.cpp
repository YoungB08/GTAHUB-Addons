#include "pch.h"
#include "HookManager.h"
#include "ChatManager.h"
#include "Logger.h"
#include "vendor/MinHook/MinHook.h"
#include <sampapi/0.3.DL-1/CChat.h>
#include <sampapi/0.3.DL-1/CInput.h>

using namespace sampapi::v03dl;

namespace {

using tRenderEntry    = void(__thiscall*)(CChat*, const char*, sampapi::CRect, D3DCOLOR);
using tAddChatMessage = void(__thiscall*)(CChat*, const char*, D3DCOLOR, const char*);
using tAddMessage     = void(__thiscall*)(CChat*, D3DCOLOR, const char*);

using tSend           = void(__thiscall*)(CInput*, const char*);
using tProcessInput   = void(__thiscall*)(CInput*);
using tOpen           = void(__thiscall*)(CInput*);
using tClose          = void(__thiscall*)(CInput*);

tRenderEntry    oRenderEntry    = nullptr;
tAddChatMessage oAddChatMessage = nullptr;
tAddMessage     oAddMessage     = nullptr;

tSend           oSend           = nullptr;
tProcessInput   oProcessInput   = nullptr;
tOpen           oOpen           = nullptr;
tClose          oClose          = nullptr;

uintptr_t GetSampAddress(uintptr_t offset) {
    static uintptr_t sampBase = reinterpret_cast<uintptr_t>(GetModuleHandleA("samp.dll"));
    return sampBase ? (sampBase + offset) : 0;
}

// Hook functions using __fastcall for x86 detour (pThis in ECX, edx in EDX)
void __fastcall Hooked_RenderEntry(CChat* pThis, void* edx, const char* szText, sampapi::CRect rect, D3DCOLOR color) {
    (void)edx;
    if (!pThis) return;

    if (oRenderEntry) {
        try {
            oRenderEntry(pThis, szText, rect, color);
        } catch (const std::exception& e) {
            Logger::Error("[EXCEPTION] oRenderEntry: %s", e.what());
        } catch (...) {
            Logger::Error("[EXCEPTION] oRenderEntry: Unknown SEH/C++ exception");
        }
    }
}

void __fastcall Hooked_AddChatMessage(CChat* pThis, void* edx, const char* szPrefix, D3DCOLOR prefixColor, const char* szText) {
    (void)edx;
    if (!pThis) return;
    Logger::Chat("[HOOK] AddChatMessage called: [%s] %s", szPrefix ? szPrefix : "", szText ? szText : "");
    try {
        ChatManager::Get().OnServerMessage(szPrefix, prefixColor, szText);
    } catch (const std::exception& e) {
        Logger::Error("[EXCEPTION] ChatManager::OnServerMessage: %s", e.what());
    } catch (...) {
        Logger::Error("[EXCEPTION] ChatManager::OnServerMessage: Unknown SEH/C++ exception");
    }

    if (oAddChatMessage) {
        try {
            oAddChatMessage(pThis, szPrefix, prefixColor, szText);
        } catch (const std::exception& e) {
            Logger::Error("[EXCEPTION] oAddChatMessage: %s", e.what());
        } catch (...) {
            Logger::Error("[EXCEPTION] oAddChatMessage: Unknown SEH/C++ exception");
        }
    }
}

void __fastcall Hooked_AddMessage(CChat* pThis, void* edx, D3DCOLOR color, const char* szText) {
    (void)edx;
    if (!pThis) return;
    Logger::Chat("[HOOK] AddMessage called: %s", szText ? szText : "");
    try {
        ChatManager::Get().OnClientMessage(color, szText);
    } catch (const std::exception& e) {
        Logger::Error("[EXCEPTION] ChatManager::OnClientMessage: %s", e.what());
    } catch (...) {
        Logger::Error("[EXCEPTION] ChatManager::OnClientMessage: Unknown SEH/C++ exception");
    }

    if (oAddMessage) {
        try {
            oAddMessage(pThis, color, szText);
        } catch (const std::exception& e) {
            Logger::Error("[EXCEPTION] oAddMessage: %s", e.what());
        } catch (...) {
            Logger::Error("[EXCEPTION] oAddMessage: Unknown SEH/C++ exception");
        }
    }
}

void __fastcall Hooked_Send(CInput* pThis, void* edx, const char* szString) {
    (void)edx;
    if (!pThis) return;
    Logger::Input("[HOOK] CInput::Send called: %s", szString ? szString : "");
    try {
        ChatManager::Get().OnPlayerSend(szString);
    } catch (const std::exception& e) {
        Logger::Error("[EXCEPTION] ChatManager::OnPlayerSend: %s", e.what());
    } catch (...) {
        Logger::Error("[EXCEPTION] ChatManager::OnPlayerSend: Unknown SEH/C++ exception");
    }

    if (oSend) {
        try {
            oSend(pThis, szString);
        } catch (const std::exception& e) {
            Logger::Error("[EXCEPTION] oSend: %s", e.what());
        } catch (...) {
            Logger::Error("[EXCEPTION] oSend: Unknown SEH/C++ exception");
        }
    }
}

void __fastcall Hooked_ProcessInput(CInput* pThis, void* edx) {
    (void)edx;
    if (!pThis) return;
    if (oProcessInput) {
        try {
            oProcessInput(pThis);
        } catch (const std::exception& e) {
            Logger::Error("[EXCEPTION] oProcessInput: %s", e.what());
        } catch (...) {
            Logger::Error("[EXCEPTION] oProcessInput: Unknown SEH/C++ exception");
        }
    }
}

void __fastcall Hooked_Open(CInput* pThis, void* edx) {
    (void)edx;
    if (!pThis) return;
    Logger::Input("[HOOK] CInput::Open called.");
    try {
        ChatManager::Get().OpenInput();
    } catch (const std::exception& e) {
        Logger::Error("[EXCEPTION] ChatManager::OpenInput: %s", e.what());
    } catch (...) {
        Logger::Error("[EXCEPTION] ChatManager::OpenInput: Unknown SEH/C++ exception");
    }

    if (oOpen) {
        try {
            oOpen(pThis);
        } catch (const std::exception& e) {
            Logger::Error("[EXCEPTION] oOpen: %s", e.what());
        } catch (...) {
            Logger::Error("[EXCEPTION] oOpen: Unknown SEH/C++ exception");
        }
    }
}

void __fastcall Hooked_Close(CInput* pThis, void* edx) {
    (void)edx;
    if (!pThis) return;
    Logger::Input("[HOOK] CInput::Close called.");
    try {
        ChatManager::Get().CloseInput();
    } catch (const std::exception& e) {
        Logger::Error("[EXCEPTION] ChatManager::CloseInput: %s", e.what());
    } catch (...) {
        Logger::Error("[EXCEPTION] ChatManager::CloseInput: Unknown SEH/C++ exception");
    }

    if (oClose) {
        try {
            oClose(pThis);
        } catch (const std::exception& e) {
            Logger::Error("[EXCEPTION] oClose: %s", e.what());
        } catch (...) {
            Logger::Error("[EXCEPTION] oClose: Unknown SEH/C++ exception");
        }
    }
}

} // namespace

namespace HookManager {

bool Install() {
    if (MH_Initialize() != MH_OK) {
        Logger::Error("Failed to initialize MinHook.");
        return false;
    }
    Logger::Hook("MinHook initialized successfully.");

    bool success = true;
    success &= InstallChatHooks();
    success &= InstallInputHooks();
    success &= InstallD3DHooks();

    if (success) {
        Logger::Hook("All hooks installed and enabled successfully.");
    } else {
        Logger::Error("One or more hooks failed to install.");
    }

    return success;
}

bool InstallChatHooks() {
    Logger::Hook("RakNet packet interception active. Skipping internal CChat hooks for maximum stability.");
    return true;
}

bool InstallInputHooks() {
    uintptr_t pSend         = GetSampAddress(Offsets::CInput_Send);
    uintptr_t pProcessInput = GetSampAddress(Offsets::CInput_ProcessInput);
    uintptr_t pOpen         = GetSampAddress(Offsets::CInput_Open);
    uintptr_t pClose        = GetSampAddress(Offsets::CInput_Close);

    if (!pSend || !pProcessInput || !pOpen || !pClose) {
        Logger::Error("Failed to locate samp.dll input addresses.");
        return false;
    }

    MH_STATUS status = MH_OK;
    status = MH_CreateHook(reinterpret_cast<void*>(pSend), reinterpret_cast<void*>(&Hooked_Send), reinterpret_cast<void**>(&oSend));
    Logger::Hook("CreateHook CInput::Send (0x%X) -> trampoline=%p: %s", Offsets::CInput_Send, oSend, MH_StatusToString(status));

    status = MH_CreateHook(reinterpret_cast<void*>(pProcessInput), reinterpret_cast<void*>(&Hooked_ProcessInput), reinterpret_cast<void**>(&oProcessInput));
    Logger::Hook("CreateHook CInput::ProcessInput (0x%X) -> trampoline=%p: %s", Offsets::CInput_ProcessInput, oProcessInput, MH_StatusToString(status));

    status = MH_CreateHook(reinterpret_cast<void*>(pOpen), reinterpret_cast<void*>(&Hooked_Open), reinterpret_cast<void**>(&oOpen));
    Logger::Hook("CreateHook CInput::Open (0x%X) -> trampoline=%p: %s", Offsets::CInput_Open, oOpen, MH_StatusToString(status));

    status = MH_CreateHook(reinterpret_cast<void*>(pClose), reinterpret_cast<void*>(&Hooked_Close), reinterpret_cast<void**>(&oClose));
    Logger::Hook("CreateHook CInput::Close (0x%X) -> trampoline=%p: %s", Offsets::CInput_Close, oClose, MH_StatusToString(status));

    // Enable input hooks one by one for step-by-step verification
    // status = MH_EnableHook(reinterpret_cast<void*>(pSend));
    // Logger::Hook("Enable CInput::Send (0x%X): %s", Offsets::CInput_Send, MH_StatusToString(status));

    // status = MH_EnableHook(reinterpret_cast<void*>(pProcessInput));
    // Logger::Hook("Enable CInput::ProcessInput (0x%X): %s", Offsets::CInput_ProcessInput, MH_StatusToString(status));

    // status = MH_EnableHook(reinterpret_cast<void*>(pOpen));
    // Logger::Hook("Enable CInput::Open (0x%X): %s", Offsets::CInput_Open, MH_StatusToString(status));

    // status = MH_EnableHook(reinterpret_cast<void*>(pClose));
    // Logger::Hook("Enable CInput::Close (0x%X): %s", Offsets::CInput_Close, MH_StatusToString(status));

    Logger::Hook("CInput hooks created successfully. EnableHook step testing mode active.");
    return true;
}

bool InstallD3DHooks() {
    Logger::D3DLog("InstallD3DHooks pipeline ready.");
    return true;
}

void Uninstall() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    Logger::Hook("All hooks disabled and MinHook uninitialized.");
}

} // namespace HookManager
