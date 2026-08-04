#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>
#include <rpc.h>
#include <rpcndr.h>

#define HUB_CORE_VERSION_MAJOR 1
#define HUB_CORE_VERSION_MINOR 1
#define HUB_CORE_VERSION_PATCH 0
#define HUB_CORE_VERSION_STRING "1.1.0"

namespace sampapi { namespace v03dl { class CNetGame; class CChat; class CInput; class CPlayerTags; class CGame; } }

inline sampapi::v03dl::CNetGame* GetRefNetGame() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return nullptr;

    auto base = reinterpret_cast<uintptr_t>(hSamp);
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    DWORD size = nt->OptionalHeader.SizeOfImage;

    uintptr_t offset = 0x21A0F8; // 0.3.7-R1
    if (size >= 0x2A0000)      offset = 0x2ACA24; // 0.3.DL-1
    else if (size >= 0x26EA00) offset = 0x26EB24; // 0.3.7-R5
    else if (size >= 0x26E000) offset = 0x26E8C4; // 0.3.7-R3

    auto ppNet = reinterpret_cast<sampapi::v03dl::CNetGame**>(base + offset);
    return (ppNet) ? *ppNet : nullptr;
}

inline sampapi::v03dl::CChat* GetRefChat() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return nullptr;

    auto base = reinterpret_cast<uintptr_t>(hSamp);
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    DWORD size = nt->OptionalHeader.SizeOfImage;

    uintptr_t offset = 0x21A0EC; // 0.3.7-R1
    if (size >= 0x2A0000)      offset = 0x2ACA10; // 0.3.DL-1
    else if (size >= 0x26EA00) offset = 0x26EB28; // 0.3.7-R5
    else if (size >= 0x26E000) offset = 0x26E8C8; // 0.3.7-R3

    auto ppChat = reinterpret_cast<sampapi::v03dl::CChat**>(base + offset);
    return (ppChat) ? *ppChat : nullptr;
}

inline sampapi::v03dl::CInput* GetRefInput() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return nullptr;

    auto base = reinterpret_cast<uintptr_t>(hSamp);
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    DWORD size = nt->OptionalHeader.SizeOfImage;

    uintptr_t offset = 0x21A0F0; // 0.3.7-R1
    if (size >= 0x2A0000)      offset = 0x2ACA14; // 0.3.DL-1
    else if (size >= 0x26EA00) offset = 0x26EB2C; // 0.3.7-R5
    else if (size >= 0x26E000) offset = 0x26E8CC; // 0.3.7-R3

    auto ppInput = reinterpret_cast<sampapi::v03dl::CInput**>(base + offset);
    return (ppInput) ? *ppInput : nullptr;
}

inline sampapi::v03dl::CGame* GetRefGame() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    if (!hSamp) return nullptr;
    const auto base = reinterpret_cast<uintptr_t>(hSamp);
    auto ppGame = reinterpret_cast<sampapi::v03dl::CGame**>(base + 0x2ACA3C);
    return ppGame ? *ppGame : nullptr;
}

inline sampapi::v03dl::CPlayerTags*& GetRefPlayerTags() {
    HMODULE hSamp = GetModuleHandleA("samp.dll");
    static sampapi::v03dl::CPlayerTags* s_NullTags = nullptr;
    if (!hSamp) return s_NullTags;

    auto base = reinterpret_cast<uintptr_t>(hSamp);
    PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return s_NullTags;
    PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
    if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return s_NullTags;

    DWORD size = nt->OptionalHeader.SizeOfImage;

    uintptr_t offset = 0x21A0B0; // 0.3.7-R1
    if (size >= 0x2A0000)      offset = 0x2AC9D8; // 0.3.DL-1
    else if (size >= 0x26EA00) offset = 0x26EB48; // 0.3.7-R5
    else if (size >= 0x26E000) offset = 0x26E890; // 0.3.7-R3

    auto ppTags = reinterpret_cast<sampapi::v03dl::CPlayerTags**>(base + offset);
    return (ppTags) ? *ppTags : s_NullTags;
}

struct SAMPVersionInfo {
    uintptr_t fnGetLocalPlayer;
    uintptr_t fnGetLocalPlayerName;
    uintptr_t fnGetName;
    uintptr_t fnGetPing;
    uintptr_t fnGetLocalPlayerPing;
    uintptr_t fnGetRakClient;

    static SAMPVersionInfo Get() {
        HMODULE hSamp = GetModuleHandleA("samp.dll");
        if (!hSamp) return {};

        auto base = reinterpret_cast<uintptr_t>(hSamp);
        PIMAGE_DOS_HEADER dos = reinterpret_cast<PIMAGE_DOS_HEADER>(base);
        if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) return {};
        PIMAGE_NT_HEADERS nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + dos->e_lfanew);
        if (!nt || nt->Signature != IMAGE_NT_SIGNATURE) return {};

        DWORD size = nt->OptionalHeader.SizeOfImage;

        SAMPVersionInfo info{};
        if (size >= 0x2A0000) { // 0.3.DL-1
            info.fnGetLocalPlayer     = base + 0x1A80;
            info.fnGetLocalPlayerName = base + 0xA1D0;
            info.fnGetName            = base + 0x170D0;
            info.fnGetPing            = base + 0x6E2B0;
            info.fnGetLocalPlayerPing = base + 0x6E2F0;
            info.fnGetRakClient       = base + 0x1A90;
        } else if (size >= 0x26EA00) { // 0.3.7-R5
            info.fnGetLocalPlayer     = base + 0x1A30;
            info.fnGetLocalPlayerName = base + 0x13D10;
            info.fnGetName            = base + 0x13D20;
            info.fnGetPing            = base + 0x6A190;
            info.fnGetLocalPlayerPing = base + 0x6A1F0;
            info.fnGetRakClient       = base + 0x1A40;
        } else if (size >= 0x26E000) { // 0.3.7-R3
            info.fnGetLocalPlayer     = base + 0x1A30;
            info.fnGetLocalPlayerName = base + 0x13CE0;
            info.fnGetName            = base + 0x13CF0;
            info.fnGetPing            = base + 0x6A130;
            info.fnGetLocalPlayerPing = base + 0x6A190;
            info.fnGetRakClient       = base + 0x1A40;
        } else { // 0.3.7-R1
            info.fnGetLocalPlayer     = base + 0x1A30;
            info.fnGetLocalPlayerName = base + 0x13CD0;
            info.fnGetName            = base + 0x13CE0;
            info.fnGetPing            = base + 0x6A130;
            info.fnGetLocalPlayerPing = base + 0x6A190;
            info.fnGetRakClient       = base + 0x1A40;
        }
        return info;
    }
};

inline void ClearLog() {
    FILE* f = nullptr;
    fopen_s(&f, "HUB-Core.log", "w");
    if (f) fclose(f);
}

inline void Log(const char* fmt, ...) {
    FILE* f = nullptr;
    fopen_s(&f, "HUB-Core.log", "a");
    if (!f) return;

    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    fputs(buf, f);
    fputs("\n", f);
    fclose(f);
}
