/**
 * @file dllmain.cpp
 * @brief DLL entry point và các patch core.
 *
 * Luồng khởi động:
 *  DLL_PROCESS_ATTACH
 *    └─ MainThread (thread)
 *         ├─ PatchVehicleLimit()          — patch ngay khi samp.dll load
 *         ├─ Chờ SAMP init xong (state=9 hoặc PlayerTags ready)
 *         ├─ D3DHook::Install()           — hook EndScene
 *         └─ Network::Init() + RequestRoles()
 *
 *  DLL_PROCESS_DETACH
 *    ├─ D3DHook::Uninstall()
 *    ├─ Network::Shutdown()
 *    └─ Nametag::Release()
 */
#include "pch.h"
#include <windows.h>
#include <psapi.h>

#include "D3DHook.h"
#include "Nametag.h"
#include "Network.h"
#include "PlayerData.h"

#include <0.3.DL-1/CNetGame.h>
#include <0.3.DL-1/CPlayerTags.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace sampapi::v03dl;

// ============================================================
// Vehicle limit patch
// Patch giới hạn 611 xe của SAMP lên 8000.
// ============================================================

static DWORD FindPattern(HMODULE hMod, const char* pattern, size_t len) {
    MODULEINFO info;
    GetModuleInformation(GetCurrentProcess(), hMod, &info, sizeof(info));
    auto base = reinterpret_cast<DWORD>(hMod);
    for (DWORD i = base; i < base + info.SizeOfImage - len; ++i)
        if (memcmp(reinterpret_cast<void*>(i), pattern, len) == 0) return i;
    return 0;
}

static void PatchVehicleLimit() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return;

    // Pattern: CMP EAX, 0x190 ... CMP EAX, 0x263 (giới hạn 611 xe)
    DWORD addr = FindPattern(hSamp,
        "\x3D\x90\x01\x00\x00\x0F\x8C\x32\x01\x00\x00"
        "\x3D\x63\x02\x00\x00\x0F\x8F\x27\x01\x00\x00", 22);
    if (!addr) return;

    // Patch hằng số 0x263 → 8000
    DWORD limitAddr = addr + 12; // offset đến DWORD của 0x263
    DWORD newLimit  = 8000;
    DWORD old;
    VirtualProtect(reinterpret_cast<LPVOID>(limitAddr), 4,
        PAGE_EXECUTE_READWRITE, &old);
    memcpy(reinterpret_cast<void*>(limitAddr), &newLimit, 4);
    VirtualProtect(reinterpret_cast<LPVOID>(limitAddr), 4, old, &old);
}

// ============================================================
// Main thread
// ============================================================

static DWORD WINAPI MainThread(LPVOID) {
    // Patch vehicle limit ngay khi samp.dll đã load
    PatchVehicleLimit();

    // Chờ SAMP khởi tạo xong PlayerTags (có D3D device)
    while (true) {
        CPlayerTags* pTags = RefPlayerTags();
        if (pTags && pTags->m_pDevice) {
            // Cài hook D3D
            if (!D3DHook::IsInstalled())
                D3DHook::Install(pTags->m_pDevice);

            // Chờ thêm đến khi có NetGame (connected)
            CNetGame* pNet = RefNetGame();
            if (pNet && pNet->GetPlayerPool() && !Network::IsReady()) {
                Network::Init();
                Network::RequestRoles(); // yêu cầu server gửi data
            }

            // Nếu cả hai đã xong thì thoát loop
            if (D3DHook::IsInstalled() && Network::IsReady()) break;
        }
        Sleep(500);
    }

    return 0;
}

// ============================================================
// DllMain
// ============================================================

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        D3DHook::Uninstall();
        Network::Shutdown();
        Nametag::Release();
        break;
    }
    return TRUE;
}
