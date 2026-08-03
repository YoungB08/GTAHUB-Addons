#include "pch.h"
#include "framework.h"
#include "D3DHook.h"
#include "HookManager.h"
#include "Logger.h"
#include "Network.h"
#include "RoleConfig.h"

#include <sampapi/0.3.DL-1/CChat.h>
#include <sampapi/0.3.DL-1/CInput.h>
#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerTags.h>
#include <chrono>
#include <cstring>
#include <psapi.h>
#include <thread>

#pragma comment(lib, "Psapi.lib")

using namespace sampapi::v03dl;

#ifndef HUB_CORE_VERSION_STRING
#define HUB_CORE_VERSION_STRING "1.1.0"
#endif

static bool s_SpawnMessageSent = false;

static uintptr_t FindPattern(HMODULE module, const unsigned char* pattern, size_t patternSize) {
    MODULEINFO moduleInfo{};
    if (!GetModuleInformation(GetCurrentProcess(), module, &moduleInfo, sizeof(moduleInfo)) ||
        moduleInfo.SizeOfImage < patternSize) {
        return 0;
    }

    const auto baseAddress = reinterpret_cast<uintptr_t>(module);
    const auto imageSize = static_cast<size_t>(moduleInfo.SizeOfImage);
    for (size_t offset = 0; offset + patternSize <= imageSize; ++offset) {
        const auto address = baseAddress + offset;
        if (std::memcmp(reinterpret_cast<const void*>(address), pattern, patternSize) == 0) {
            return address;
        }
    }

    return 0;
}

static bool PatchVehicleLimit() {
    HMODULE sampModule = GetModuleHandleA("samp.dll");
    if (!sampModule) {
        return false;
    }

    static constexpr unsigned char pattern[] = {
        0x3D, 0x90, 0x01, 0x00, 0x00, 0x0F, 0x8C, 0x32, 0x01, 0x00, 0x00,
        0x3D, 0x63, 0x02, 0x00, 0x00, 0x0F, 0x8F, 0x27, 0x01, 0x00, 0x00
    };

    const uintptr_t patternAddress = FindPattern(sampModule, pattern, sizeof(pattern));
    if (!patternAddress) {
        Logger::Info("Vehicle limit patch: SAMP pattern not found.");
        return true;
    }

    constexpr DWORD newLimit = 8000;
    const uintptr_t limitAddress = patternAddress + 12;
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(limitAddress), sizeof(newLimit), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        Logger::Error("Vehicle limit patch: VirtualProtect failed.");
        return true;
    }

    std::memcpy(reinterpret_cast<void*>(limitAddress), &newLimit, sizeof(newLimit));
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(limitAddress), sizeof(newLimit));

    DWORD unusedProtect = 0;
    VirtualProtect(reinterpret_cast<void*>(limitAddress), sizeof(newLimit), oldProtect, &unusedProtect);
    Logger::Info("Vehicle limit patch applied: 611 -> %lu.", static_cast<unsigned long>(newLimit));
    return true;
}

static DWORD WINAPI MainThread(LPVOID lpParam) {
    (void)lpParam;
    Logger::ClearLog();
    Logger::Info("=================================================================");
    Logger::Info("   GTAHUB Client Core (HUB-Core.asi v%s) Initializing...", HUB_CORE_VERSION_STRING);
    Logger::Info("=================================================================");

    RoleConfig::InitDefaults();
    bool vehicleLimitPatchAttempted = false;
    bool hooksInstalledAttempted = false;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        if (!vehicleLimitPatchAttempted) {
            vehicleLimitPatchAttempted = PatchVehicleLimit();
        }

        CChat* pChat = GetRefChat();
        CInput* pInput = GetRefInput();
        if (!hooksInstalledAttempted && pChat && pInput) {
            hooksInstalledAttempted = true;
            HookManager::Install();
        }

        CNetGame* netGame = GetRefNetGame();
        CPlayerTags* playerTags = GetRefPlayerTags();
        if (playerTags && playerTags->m_pDevice && !D3DHook::IsInstalled()) {
            D3DHook::Install(playerTags->m_pDevice);
        }

        if (netGame && !Network::IsReady()) {
            Network::Init();
        }

        if (pChat && !s_SpawnMessageSent) {
            pChat->AddMessage(D3DCOLOR_ARGB(255, 0, 255, 0),
                "[HUB-Core] HUBCore.asi Version: " HUB_CORE_VERSION_STRING);
            s_SpawnMessageSent = true;
            Logger::Info("Sent CChat startup message: HUBCore.asi Version %s", HUB_CORE_VERSION_STRING);
        } else if (!pChat) {
            s_SpawnMessageSent = false;
        }
    }

    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    (void)lpReserved;
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
            break;
        case DLL_PROCESS_DETACH:
            HookManager::Uninstall();
            Network::Shutdown();
            D3DHook::Uninstall();
            break;
    }
    return TRUE;
}
