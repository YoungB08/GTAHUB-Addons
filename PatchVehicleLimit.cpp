#include <Windows.h>
#include <Psapi.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#pragma comment(lib, "Psapi.lib")

namespace {

constexpr DWORD kVehicleLimit = 8000;
constexpr uintptr_t kNetGamePointerOffset = 0x2ACA24;

#pragma pack(push, 1)
struct CNetGame {
    char pad_0[44];
    void* m_pRakClient;
    char m_szHostAddress[257];
};
#pragma pack(pop)

static_assert(offsetof(CNetGame, m_szHostAddress) == 48,
              "Unexpected CNetGame host-address offset");

uintptr_t FindPattern(HMODULE module, const unsigned char* pattern, size_t patternSize) {
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

bool PatchVehicleLimit() {
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
        return true;
    }

    const uintptr_t limitAddress = patternAddress + 12;
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(limitAddress), sizeof(kVehicleLimit),
                        PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return true;
    }

    std::memcpy(reinterpret_cast<void*>(limitAddress), &kVehicleLimit, sizeof(kVehicleLimit));
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(limitAddress),
                          sizeof(kVehicleLimit));

    DWORD unusedProtect = 0;
    VirtualProtect(reinterpret_cast<void*>(limitAddress), sizeof(kVehicleLimit), oldProtect,
                   &unusedProtect);
    return true;
}

CNetGame* RefNetGame() {
    HMODULE sampModule = GetModuleHandleA("samp.dll");
    if (!sampModule) {
        return nullptr;
    }

    const auto baseAddress = reinterpret_cast<uintptr_t>(sampModule);
    return *reinterpret_cast<CNetGame**>(baseAddress + kNetGamePointerOffset);
}

DWORD WINAPI CheckHostAddressThread(LPVOID) {
    while (true) {
        CNetGame* netGame = RefNetGame();
        if (netGame && netGame->m_szHostAddress[0] != '\0') {
            const char* hostAddress = netGame->m_szHostAddress;
            const bool isAllowed = std::strcmp(hostAddress, "127.0.0.1") == 0 ||
                                   std::strcmp(hostAddress, "26.42.80.113") == 0;

            if (!isAllowed) {
                MessageBoxA(nullptr, "Neu muon choi may chu khac hay xoa HUB-Core.asi",
                            "GTAHUB-Dev", MB_OK | MB_ICONERROR);
                Sleep(1000);
                ExitProcess(0);
            }

            break;
        }

        Sleep(500);
    }

    return 0;
}

DWORD WINAPI InitializeAndLoad(LPVOID) {
    while (*reinterpret_cast<volatile unsigned char*>(0xC8D4C0) != 9) {
        Sleep(100);
    }

    return CheckHostAddressThread(nullptr);
}

DWORD WINAPI PatchThread(LPVOID) {
    while (!PatchVehicleLimit()) {
        Sleep(250);
    }

    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        HANDLE patchThread = CreateThread(nullptr, 0, PatchThread, nullptr, 0, nullptr);
        if (patchThread) {
            CloseHandle(patchThread);
        }

        HANDLE hostCheckThread = CreateThread(nullptr, 0, InitializeAndLoad, nullptr, 0, nullptr);
        if (hostCheckThread) {
            CloseHandle(hostCheckThread);
        }
    }

    return TRUE;
}
