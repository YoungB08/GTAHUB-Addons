#include "pch.h"
#include "HookManager.h"

#include "ChatManager.h"
#include "CustomChat.h"
#include "CustomChatInput.h"
#include "Logger.h"
#include "Network.h"
#include "SettingsPanel.h"
#include "vendor/MinHook/MinHook.h"

#include <sampapi/0.3.DL-1/CChat.h>
#include <sampapi/0.3.DL-1/CInput.h>
#include <sampapi/0.3.DL-1/CLocalPlayer.h>
#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerPool.h>

#include <atomic>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

using namespace sampapi::v03dl;

namespace {

using OpenFn = void(__thiscall*)(CInput*);
using CloseFn = void(__thiscall*)(CInput*);

struct SampEntrySnapshot {
    int timestamp = 0;
    int type = 0;
    std::string prefix;
    std::string text;
    D3DCOLOR textColor = 0;
    D3DCOLOR prefixColor = 0;
};

OpenFn g_OriginalOpen = nullptr;
CloseFn g_OriginalClose = nullptr;
std::atomic<bool> g_Installed{false};
std::atomic<bool> g_NativeChatSuppressed{false};
std::atomic<bool> g_NativeInputOpen{false};
CChat* g_SuppressedChat = nullptr;
int g_PreviousChatMode = CChat::DISPLAY_MODE_NORMAL;
CChat* g_SyncedChat = nullptr;
std::vector<SampEntrySnapshot> g_PreviousSampEntries;

uintptr_t GetSampAddress(uintptr_t offset) {
    const HMODULE module = GetModuleHandleA("samp.dll");
    return module ? reinterpret_cast<uintptr_t>(module) + offset : 0;
}

bool IsExecutableAddress(uintptr_t address) {
    if (address < 0x10000) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(reinterpret_cast<const void*>(address), &information, sizeof(information)) != sizeof(information) ||
        information.State != MEM_COMMIT || (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    constexpr DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (information.Protect & executable) != 0;
}

bool IsSupportedSampVersion() {
    const HMODULE module = GetModuleHandleA("samp.dll");
    if (!module) return false;
    const auto base = reinterpret_cast<uintptr_t>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    const DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    return imageSize >= 0x2A0000;
}

bool CreateAndEnable(uintptr_t offset, void* detour, void** original, const char* name) {
    const uintptr_t address = GetSampAddress(offset);
    if (!IsExecutableAddress(address)) {
        Logger::Error("Hook target %s is not executable. offset=0x%X", name, static_cast<unsigned>(offset));
        return false;
    }

    MH_STATUS status = MH_CreateHook(reinterpret_cast<void*>(address), detour, original);
    if (status != MH_OK) {
        Logger::Error("MH_CreateHook %s failed: %s", name, MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(reinterpret_cast<void*>(address));
    if (status != MH_OK) {
        Logger::Error("MH_EnableHook %s failed: %s", name, MH_StatusToString(status));
        return false;
    }
    Logger::Hook("Enabled %s at samp.dll+0x%X", name, static_cast<unsigned>(offset));
    return true;
}

bool SameEntry(const SampEntrySnapshot& left, const SampEntrySnapshot& right) {
    return left.timestamp == right.timestamp && left.type == right.type &&
        left.prefix == right.prefix && left.text == right.text &&
        left.textColor == right.textColor && left.prefixColor == right.prefixColor;
}

template <size_t Size>
std::string BoundedString(const char (&text)[Size]) {
    size_t length = 0;
    while (length < Size && text[length] != '\0') ++length;
    return std::string(text, length);
}

bool CopySampEntries(CChat* chat, CChat::ChatEntry* entries) {
    if (!chat || !entries) return false;
#if defined(_MSC_VER)
    __try {
        std::memcpy(entries, chat->m_entry, sizeof(chat->m_entry));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    std::memcpy(entries, chat->m_entry, sizeof(chat->m_entry));
    return true;
#endif
}

void __fastcall HookedOpen(CInput* input, void*) {
    if (!input) return;
    if (HUB::Chat::CustomChat::IsReady()) {
        HUB::Chat::CustomChat::OpenInput();
        return;
    }
    if (g_OriginalOpen) {
        g_OriginalOpen(input);
        g_NativeInputOpen.store(true, std::memory_order_release);
    }
}

void __fastcall HookedClose(CInput* input, void*) {
    if (!input) return;
    HUB::Chat::CustomChat::CloseInput();
    if (g_NativeInputOpen.exchange(false, std::memory_order_acq_rel) && g_OriginalClose) {
        g_OriginalClose(input);
    }
}

} // namespace

namespace HookManager {

bool Install() {
    if (g_Installed.load(std::memory_order_acquire)) return true;
    if (!IsSupportedSampVersion()) {
        Logger::Error("Custom Chat only supports SA-MP 0.3.DL R1; hooks were not installed.");
        return false;
    }
    const MH_STATUS initializeStatus = MH_Initialize();
    if (initializeStatus != MH_OK && initializeStatus != MH_ERROR_ALREADY_INITIALIZED) {
        Logger::Error("Failed to initialize MinHook: %s", MH_StatusToString(initializeStatus));
        return false;
    }

    if (!InstallChatHooks() || !InstallInputHooks() || !InstallD3DHooks()) {
        Logger::Error("Custom Chat hook installation failed; rolling back to native chat.");
        Uninstall();
        return false;
    }
    g_Installed.store(true, std::memory_order_release);
    Logger::Hook("Custom Chat hooks installed successfully.");
    return true;
}

bool InstallChatHooks() {
    return true;
}

bool InstallInputHooks() {
    return CreateAndEnable(Offsets::CInput_Open,
               reinterpret_cast<void*>(&HookedOpen),
               reinterpret_cast<void**>(&g_OriginalOpen), "CInput::Open") &&
        CreateAndEnable(Offsets::CInput_Close,
               reinterpret_cast<void*>(&HookedClose),
               reinterpret_cast<void**>(&g_OriginalClose), "CInput::Close");
}

bool InstallD3DHooks() {
    return true;
}

bool IsInstalled() {
    return g_Installed.load(std::memory_order_acquire);
}

bool SendChatText(const char* text) {
    CInput* input = GetRefInput();
    if (!input || !text || text[0] == '\0') return false;
    try {
        HUB::Chat::ChatManager::Get().OnPlayerSend(text);

        if (text[0] != '/') {
            CNetGame* netGame = GetRefNetGame();
            CPlayerPool* playerPool = netGame ? netGame->GetPlayerPool() : nullptr;
            CLocalPlayer* localPlayer = playerPool ? playerPool->GetLocalPlayer() : nullptr;
            if (localPlayer) {
                localPlayer->Chat(text);
                return true;
            }

            if (input->m_pDefaultCommand) {
                input->m_pDefaultCommand(text);
                return true;
            }
            return false;
        }

        std::string commandLine(text + 1);
        const size_t separator = commandLine.find(' ');
        const std::string command = commandLine.substr(0, separator);

        if (_stricmp(command.c_str(), "settings") == 0 || _stricmp(command.c_str(), "setting") == 0) {
            HUB::Chat::SettingsPanel::Open();
            return true;
        }

        const auto handler = input->GetCommandHandler(command.c_str());
        if (handler) {
            const char* arguments = separator == std::string::npos
                ? ""
                : commandLine.c_str() + separator + 1;
            handler(arguments);
        } else {
            input->Send(text);
        }
        return true;
    } catch (const std::exception& exception) {
        Logger::Error("Failed to dispatch chat input: %s", exception.what());
    } catch (...) {
        Logger::Error("Failed to dispatch chat input with unknown exception.");
    }
    return false;
}

void CloseChatInput() {
    CInput* input = GetRefInput();
    HUB::Chat::CustomChat::CloseInput();
    if (input && g_NativeInputOpen.exchange(false, std::memory_order_acq_rel) && g_OriginalClose) {
        g_OriginalClose(input);
    }
}

bool SetNativeChatSuppressed(bool suppressed) {
    CChat* chat = GetRefChat();
    if (suppressed) {
        if (!chat) return false;
        if (g_SuppressedChat != chat) {
            g_SuppressedChat = chat;
            g_PreviousChatMode = chat->m_nMode;
        }
        chat->m_nMode = CChat::DISPLAY_MODE_OFF;
    } else if (g_SuppressedChat) {
        if (chat == g_SuppressedChat) chat->m_nMode = g_PreviousChatMode;
        g_SuppressedChat = nullptr;
    }

    const bool changed = g_NativeChatSuppressed.exchange(suppressed, std::memory_order_acq_rel) != suppressed;
    if (changed) Logger::Hook("Native chat suppression %s.", suppressed ? "enabled" : "disabled");
    return true;
}

void RegisterNativeCommands() {
    static bool registered = false;
    if (registered) return;
    CInput* input = GetRefInput();
    if (!input) return;

    auto cmdHandler = [](const char*) {
        HUB::Chat::SettingsPanel::Open();
    };

    input->AddCommand("settings", cmdHandler);
    input->AddCommand("setting", cmdHandler);
    registered = true;
    Logger::Info("Registered /settings and /setting commands into CInput.");
}

void SyncSampChat() {
    RegisterNativeCommands();
    CChat* chat = GetRefChat();
    if (!chat) {
        g_SyncedChat = nullptr;
        g_PreviousSampEntries.clear();
        return;
    }

    std::array<CChat::ChatEntry, CChat::MAX_MESSAGES> rawEntries{};
    if (!CopySampEntries(chat, rawEntries.data())) {
        Logger::Error("Failed to read the native SA-MP chat history.");
        return;
    }

    std::vector<SampEntrySnapshot> currentEntries;
    currentEntries.reserve(rawEntries.size());
    for (const CChat::ChatEntry& entry : rawEntries) {
        SampEntrySnapshot snapshot;
        snapshot.timestamp = entry.m_timestamp;
        snapshot.type = entry.m_nType;
        snapshot.prefix = BoundedString(entry.m_szPrefix);
        snapshot.text = BoundedString(entry.m_szText);
        snapshot.textColor = entry.m_textColor;
        snapshot.prefixColor = entry.m_prefixColor;
        if (snapshot.type == CChat::ENTRY_TYPE_NONE && snapshot.prefix.empty() && snapshot.text.empty()) continue;
        currentEntries.push_back(std::move(snapshot));
    }

    if (g_SyncedChat != chat) {
        g_SyncedChat = chat;
        g_PreviousSampEntries.clear();
    }

    size_t overlap = (std::min)(g_PreviousSampEntries.size(), currentEntries.size());
    while (overlap > 0 && !std::equal(
        g_PreviousSampEntries.end() - static_cast<std::ptrdiff_t>(overlap),
        g_PreviousSampEntries.end(), currentEntries.begin(), SameEntry)) {
        --overlap;
    }

    for (size_t index = overlap; index < currentEntries.size(); ++index) {
        const SampEntrySnapshot& entry = currentEntries[index];
        HUB::Chat::ChatManager::Get().OnSampEntry(entry.type, entry.text.c_str(), entry.prefix.c_str(),
            entry.textColor, entry.prefixColor);
    }
    g_PreviousSampEntries = std::move(currentEntries);
}

void Uninstall() {
    g_Installed.store(false, std::memory_order_release);
    HUB::Chat::CustomChat::CloseInput();
    SetNativeChatSuppressed(false);
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    g_NativeChatSuppressed.store(false, std::memory_order_release);
    g_SuppressedChat = nullptr;
    g_SyncedChat = nullptr;
    g_PreviousSampEntries.clear();
    g_OriginalOpen = nullptr;
    g_OriginalClose = nullptr;
    g_NativeInputOpen.store(false, std::memory_order_release);
    Logger::Hook("Custom Chat hooks uninstalled.");
}

} // namespace HookManager
