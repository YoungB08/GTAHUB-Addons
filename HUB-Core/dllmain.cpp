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

#include <sampapi/0.3.DL-1/CChat.h>
#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerTags.h>

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

    // Patch hằng số 0x263 thành 8000
    DWORD limitAddr = addr + 12; // offset dẫn DWORD của 0x263
    DWORD newLimit  = 8000;
    DWORD old;
    VirtualProtect(reinterpret_cast<LPVOID>(limitAddr), 4,
        PAGE_EXECUTE_READWRITE, &old);
    memcpy(reinterpret_cast<void*>(limitAddr), &newLimit, 4);
    VirtualProtect(reinterpret_cast<LPVOID>(limitAddr), 4, old, &old);
}

#include "RoleConfig.h"

// ============================================================
// Main thread
// ============================================================

static DWORD WINAPI MainThread(LPVOID) {
    Log("MainThread started.");
    RoleConfig::InitDefaults();

    int loopCount = 0;
    static bool s_bSpawnMessageSent = false;

    while (true) {
        Sleep(500); 
        loopCount++;

        CNetGame* pNetGame = GetRefNetGame();

        // 1. Kiểm tra IP khi CNetGame đã sẵn sàng và HostAddress đã được ghi nhận
        if (pNetGame && pNetGame->m_szHostAddress && strlen(pNetGame->m_szHostAddress) > 0) {
            if (strcmp(pNetGame->m_szHostAddress, "127.0.0.1") != 0 && 
                strcmp(pNetGame->m_szHostAddress, "26.42.80.113") != 0) {
                
                MessageBoxA(0, "Neu muon choi may chu khac hay xoa HUB-Core.asi", "GTAHUB-Dev", MB_OK | MB_ICONERROR);
                Sleep(500);
                ExitProcess(0);
            }
        }

        auto ppDev = reinterpret_cast<IDirect3DDevice9**>(0xC97C28);
        IDirect3DDevice9* dev = (ppDev && !IsBadReadPtr(ppDev, sizeof(void*)) && *ppDev && !IsBadReadPtr(*ppDev, sizeof(void*))) ? *ppDev : nullptr;

        if (dev) {
            // Cài/Cập nhật hook D3D bất cứ khi nào device thay đổi
            D3DHook::Install(dev);

            // Chờ có NetGame đã kết nối xong (GAME_MODE_CONNECTED = 5) mới hook Network
            if (pNetGame && pNetGame->GetState() == 5 && !Network::IsReady()) {
                Log("MainThread: Init Network with pNet %p (State=%d)", pNetGame, pNetGame->GetState());
                Network::Init();
            }
        }

        // 2. Gửi tin nhắn CChat và khởi tạo Test Roles khi người chơi Spawn vào game
        if (pNetGame && pNetGame->GetPlayerPool()) {
            CPlayerPool* pPlayerPool = pNetGame->GetPlayerPool();
            CLocalPlayer* pLocalPlayer = pPlayerPool ? pPlayerPool->GetLocalPlayer() : nullptr;

            if (pLocalPlayer && pLocalPlayer->m_bIsActive) {
                if (!s_bSpawnMessageSent) {
                    // Gửi request data cho Server sau khi đã kết nối và spawn xong
                    if (Network::IsReady()) {
                        Network::RequestData();
                    }

                    CChat* pChat = GetRefChat();
                    if (pChat) {
                        pChat->AddMessage(0x00FF00FF, "[HUB-Core] HUBCore.asi Version: " HUB_CORE_VERSION_STRING);
                        pChat->AddMessage(0xFF3399FF, "[HUB-Core] Auto Test Roles initialized from HUB-Roles.json!");
                        s_bSpawnMessageSent = true;
                        Log("Sent spawn message to CChat: HUBCore.asi Version %s", HUB_CORE_VERSION_STRING);
                    }

                    // Tự động nạp 3 Role Test từ HUB-Roles.json để kiểm tra 3 role mỗi hàng ngay trong game
                    uint16_t localId = pPlayerPool->m_nLocalPlayerId;
                    for (int id = 0; id < 20; id++) {
                        RoleConfig::RolePresetConfig cfg1 = RoleConfig::GetPresetRoleConfig((id % 2 == 0) ? "ADMIN" : "DEV");
                        RoleConfig::RolePresetConfig cfg2 = RoleConfig::GetPresetRoleConfig("VIP");
                        RoleConfig::RolePresetConfig cfg3 = RoleConfig::GetPresetRoleConfig((id % 2 == 0) ? "MOD" : "HELPER");

                        g_Players[id].tags[0] = { cfg1.text, cfg1.color, cfg1.stroke };
                        g_Players[id].tags[1] = { cfg2.text, cfg2.color, cfg2.stroke };
                        g_Players[id].tags[2] = { cfg3.text, cfg3.color, cfg3.stroke };
                        g_Players[id].tagCount = 3;

                        if (id == localId || id % 2 == 0) {
                            g_Players[id].iconUrl = cfg1.imagePath;
                        } else {
                            g_Players[id].iconUrl.clear();
                        }
                        g_Players[id].hasData = true;
                    }
                    Log("Auto test 3-roles per row assigned to local player ID %d and slots 0-19", localId);
                }
            } else {
                // Reset flag nếu player chuyển trạng thái (chưa spawn / reconnect / back to class selection)
                s_bSpawnMessageSent = false;
            }
        }

        if (loopCount % 20 == 0) {
            Log("MainThread loop %d: dev=%p, D3DHookInstalled=%d, NetworkReady=%d",
                loopCount, dev, (int)D3DHook::IsInstalled(), (int)Network::IsReady());
        }
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
