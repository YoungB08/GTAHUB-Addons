#include "pch.h"
#include <windows.h>
#include <psapi.h>
#include <0.3.DL-1/CNetGame.h>
#pragma comment(lib, "Ws2_32.lib")

using namespace sampapi::v03dl;

DWORD FindAddress(HMODULE hModule, const char* pattern, size_t patternSize) {
    MODULEINFO moduleInfo;
    DWORD baseAddress = reinterpret_cast<DWORD>(hModule);
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));

    for (DWORD i = baseAddress; i < baseAddress + moduleInfo.SizeOfImage - patternSize; ++i) {
        if (memcmp(reinterpret_cast<void*>(i), pattern, patternSize) == 0) {
            return i;
        }
    }
    return 0;
}
void MemoryFill(DWORD address, BYTE value, size_t size) {
    DWORD oldProtect;
    VirtualProtect((LPVOID)address, size, PAGE_EXECUTE_READWRITE, &oldProtect);
    memset((void*)address, value, size);
    VirtualProtect((LPVOID)address, size, oldProtect, &oldProtect);
}

// Patch giới hạn số lượng xe
void PatchVehicleLimit() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return;

    // Tìm pattern chứa lệnh: CMP EAX, 0x190 và CMP EAX, 0x263
    DWORD patternAddr = FindAddress(hSamp,
        "\x3D\x90\x01\x00\x00\x0F\x8C\x32\x01\x00\x00\x3D\x63\x02\x00\x00\x0F\x8F\x27\x01\x00\x00",
        22);

    if (patternAddr) {
        DWORD addrLimit = patternAddr + 11 + 1; // vị trí hằng số 0x263 (611)
        DWORD newLimit = 8000;

        DWORD oldProtect;
        VirtualProtect((LPVOID)addrLimit, 4, PAGE_EXECUTE_READWRITE, &oldProtect);
        memcpy((void*)addrLimit, &newLimit, 4);
        VirtualProtect((LPVOID)addrLimit, 4, oldProtect, &oldProtect);
    }
}


DWORD WINAPI CheckHostAddressThread(LPVOID) {
    while (true) {
        CNetGame* pNetGame = RefNetGame();

        //if (pNetGame && strlen(pNetGame->m_szHostAddress) > 0) {
        //    if (strcmp(pNetGame->m_szHostAddress, "127.0.0.1") != 0 && strcmp(pNetGame->m_szHostAddress, "26.42.80.113") != 0) {
        //        MessageBoxA(0, "Neu muon choi may chu khac hay xoa gtahub.asi", "GTAHUB-Dev", MB_OK | MB_ICONERROR);
        //        Sleep(1000);
        //        ExitProcess(0);
        //    }
        //    break; // địa chỉ đúng => thoát loop
        //}

        Sleep(500); // chờ NetGame được init
    }

    return 0;
}

DWORD WINAPI InitializeAndLoad(LPVOID) {
    while (*reinterpret_cast<unsigned char*>(0xC8D4C0) != 9) {
        Sleep(100);
    }
    CreateThread(0, 0, &CheckHostAddressThread, 0, 0, 0);
    return 0;
}
DWORD WINAPI MainThread(LPVOID lpParam) {

    PatchVehicleLimit();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
        CreateThread(0, 0, &InitializeAndLoad, 0, 0, 0);
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}