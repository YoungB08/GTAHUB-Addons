/**
 * @file dllmain.cpp
 * @brief DLL entry point và các patch core.
 *
 * Lu?ng kh?i d?ng:
 *  DLL_PROCESS_ATTACH
 *    +- MainThread (thread)
 *         +- PatchVehicleLimit()          — patch ngay khi samp.dll load
 *         +- Ch? SAMP init xong (state=9 ho?c PlayerTags ready)
 *         +- D3DHook::Install()           — hook EndScene
 *         +- Network::Init() + RequestRoles()
 *
 *  DLL_PROCESS_DETACH
 *    +- D3DHook::Uninstall()
 *    +- Network::Shutdown()
 *    +- Nametag::Release()
 */
#include "pch.h"
#include <windows.h>
#include <psapi.h>

#include "D3DHook.h"
#include "Nametag.h"
#include "Network.h"
#include "PlayerData.h"

#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerTags.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace sampapi::v03dl;

// ============================================================
// Vehicle limit patch
// Patch gi?i h?n 611 xe c?a SAMP lên 8000.
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

    // Pattern: CMP EAX, 0x190 ... CMP EAX, 0x263 (gi?i h?n 611 xe)
    DWORD addr = FindPattern(hSamp,
        "\x3D\x90\x01\x00\x00\x0F\x8C\x32\x01\x00\x00"
        "\x3D\x63\x02\x00\x00\x0F\x8F\x27\x01\x00\x00", 22);
    if (!addr) return;

    // Patch h?ng s? 0x263 ? 8000
    DWORD limitAddr = addr + 12; // offset d?n DWORD c?a 0x263
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
    Log("MainThread started.");

    int loopCount = 0;
    while (true) {
        loopCount++;
        auto ppDev = reinterpret_cast<IDirect3DDevice9**>(0xC97C28);
        IDirect3DDevice9* dev = (ppDev && !IsBadReadPtr(ppDev, sizeof(void*)) && *ppDev && !IsBadReadPtr(*ppDev, sizeof(void*))) ? *ppDev : nullptr;

        if (dev) {
            // Cài/Cập nhật hook D3D bất cứ khi nào device thay đổi
            D3DHook::Install(dev);

            // Chờ có NetGame (connected)
            CNetGame* pNet = GetRefNetGame();
            if (pNet && pNet->GetPlayerPool() && !Network::IsReady()) {
                Log("MainThread: Init Network with pNet %p", pNet);
                Network::Init();
                Network::RequestData(); // yêu cầu server gửi data
            }
        }
        if (loopCount % 20 == 0) {
            Log("MainThread loop %d: dev=%p, D3DHookInstalled=%d, NetworkReady=%d",
                loopCount, dev, (int)D3DHook::IsInstalled(), (int)Network::IsReady());
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
        Log("=== HUB-Core.asi DLL_PROCESS_ATTACH ===");
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        break;

    case DLL_PROCESS_DETACH:
        Log("=== HUB-Core.asi DLL_PROCESS_DETACH ===");
        D3DHook::Uninstall();
        Network::Shutdown();
        Nametag::Release();
        break;
    }
    return TRUE;
}
