/**
 * @file dllmain.cpp
 * @brief Entry point DLL cho client ASI GTA SA (HUB-Core.asi).
 */
#include "pch.h"
#include "D3DHook.h"
#include "Network.h"
#include "PlayerData.h"
#include "RoleConfig.h"
#include "TextureCache.h"

#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerTags.h>
#include <sampapi/0.3.DL-1/CChat.h>
#include <thread>
#include <chrono>

using namespace sampapi::v03dl;

#ifndef HUB_CORE_VERSION_STRING
#define HUB_CORE_VERSION_STRING "4.0.0 (open:mp Multi-Slot Async)"
#endif

static bool s_bSpawnMessageSent = false;

static DWORD WINAPI MainThread(LPVOID lpParam) {
    (void)lpParam;
    Log("=================================================================");
    Log("   GTAHUB Client Core (HUB-Core.asi v%s) Initializing...", HUB_CORE_VERSION_STRING);
    Log("=================================================================");

    RoleConfig::InitDefaults();

    int loopCount = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        loopCount++;

        CNetGame* pNet = GetRefNetGame();
        if (pNet) {
            if (!D3DHook::IsInstalled()) {
                sampapi::v03dl::CPlayerTags* pTags = sampapi::v03dl::RefPlayerTags();
                if (pTags && pTags->m_pDevice) {
                    D3DHook::Install(pTags->m_pDevice);
                    Log("D3DHook installed successfully.");
                }
            }

            if (!Network::IsReady()) {
                Network::Init();
                if (Network::IsReady()) {
                    Log("Network initialized.");
                }
            }

            CPlayerPool* pPlayerPool = pNet->GetPlayerPool();
            if (pPlayerPool) {
                CLocalPlayer* pLocalPlayer = pPlayerPool->GetLocalPlayer();
                if (pLocalPlayer && pLocalPlayer->m_bIsActive) {
                    if (!s_bSpawnMessageSent) {
                        if (Network::IsReady()) {
                            Network::RequestData();
                        }

                        CChat* pChat = GetRefChat();
                        if (pChat) {
                            pChat->AddMessage(0x00FF00FF, "[HUB-Core] HUBCore.asi Version: " HUB_CORE_VERSION_STRING);
                            s_bSpawnMessageSent = true;
                            Log("Sent spawn message to CChat: HUBCore.asi Version %s", HUB_CORE_VERSION_STRING);
                        }
                    }
                } else {
                    s_bSpawnMessageSent = false;
                }
            }
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
            Network::Shutdown();
            D3DHook::Uninstall();
            TextureCache::ReleaseAll();
            break;
    }
    return TRUE;
}
