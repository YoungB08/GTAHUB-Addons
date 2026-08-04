#include "pch.h"
#include "ChatSettings.h"

#include "Logger.h"

#include <algorithm>
#include <cwchar>
#include <iterator>

namespace {

int ReadInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, int fallback) {
    return GetPrivateProfileIntW(section, key, fallback, path.c_str());
}

float ReadFloat(const std::wstring& path, const wchar_t* section, const wchar_t* key, float fallback) {
    wchar_t buffer[64]{};
    wchar_t fallbackText[64]{};
    swprintf_s(fallbackText, L"%.3f", fallback);
    GetPrivateProfileStringW(section, key, fallbackText, buffer,
        static_cast<DWORD>(std::size(buffer)), path.c_str());
    wchar_t* end = nullptr;
    const float value = std::wcstof(buffer, &end);
    return end == buffer ? fallback : value;
}

D3DCOLOR ReadColor(const std::wstring& path, const wchar_t* key, D3DCOLOR fallback) {
    wchar_t buffer[32]{};
    wchar_t fallbackText[32]{};
    swprintf_s(fallbackText, L"%08X", fallback);
    GetPrivateProfileStringW(L"Colors", key, fallbackText, buffer,
        static_cast<DWORD>(std::size(buffer)), path.c_str());
    wchar_t* end = nullptr;
    const unsigned long value = std::wcstoul(buffer, &end, 16);
    return end == buffer ? fallback : static_cast<D3DCOLOR>(value);
}

bool GetWriteTime(const std::wstring& path, FILETIME& writeTime) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return false;
    writeTime = data.ftLastWriteTime;
    return true;
}

} // namespace

namespace HUB::Chat {

ChatSettingsStore& GetSettingsStore() {
    static ChatSettingsStore instance;
    return instance;
}

bool ChatSettingsStore::Initialize() {
    path_ = L"HUB-Core\\chat.ini";
    return Load();
}

bool ChatSettingsStore::ReloadIfChanged() {
    const DWORD now = GetTickCount();
    if (now - lastCheckTick_ < 1000) return false;
    lastCheckTick_ = now;

    FILETIME writeTime{};
    if (!GetWriteTime(path_, writeTime) || CompareFileTime(&writeTime, &lastWriteTime_) == 0) return false;
    return Load();
}

const ChatSettings& ChatSettingsStore::Get() const {
    return settings_;
}

uint64_t ChatSettingsStore::Generation() const {
    return generation_;
}

bool ChatSettingsStore::Load() {
    ChatSettings next;
    next.scale = std::clamp(ReadFloat(path_, L"Layout", L"Scale", next.scale), 0.8f, 1.5f);
    next.width = std::clamp(ReadInt(path_, L"Layout", L"Width", next.width), 320, 900);
    next.height = std::clamp(ReadInt(path_, L"Layout", L"Height", next.height), 180, 600);
    next.fontSize = std::clamp(ReadInt(path_, L"Layout", L"FontSize", next.fontSize), 10, 24);
    next.lineHeight = std::clamp(ReadInt(path_, L"Layout", L"LineHeight", next.lineHeight), 14, 36);
    next.composerHeight = std::clamp(ReadInt(path_, L"Layout", L"ComposerHeight", next.composerHeight), 28, 60);
    next.padding = std::clamp(ReadInt(path_, L"Layout", L"Padding", next.padding), 4, 24);
    next.marginLeft = std::clamp(ReadInt(path_, L"Layout", L"MarginLeft", next.marginLeft), 0, 200);
    next.marginTop = std::clamp(ReadInt(path_, L"Layout", L"MarginTop", next.marginTop), 0, 200);

    next.opacity = std::clamp(ReadInt(path_, L"Behavior", L"Opacity", next.opacity), 0, 255);
    next.idleOpacity = std::clamp(ReadInt(path_, L"Behavior", L"IdleOpacity", next.idleOpacity), 0, 255);
    next.fadeDelayMs = std::clamp(ReadInt(path_, L"Behavior", L"FadeDelayMs", next.fadeDelayMs), 0, 60000);
    next.wheelLines = std::clamp(ReadInt(path_, L"Behavior", L"WheelLines", next.wheelLines), 1, 10);
    next.timestamps = ReadInt(path_, L"Behavior", L"Timestamps", next.timestamps ? 1 : 0) != 0;
    next.smoothScroll = ReadInt(path_, L"Behavior", L"SmoothScroll", next.smoothScroll ? 1 : 0) != 0;
    next.fadeEnabled = ReadInt(path_, L"Behavior", L"FadeEnabled", next.fadeEnabled ? 1 : 0) != 0;

    next.backgroundColor = ReadColor(path_, L"Background", next.backgroundColor);
    next.composerColor = ReadColor(path_, L"Composer", next.composerColor);
    next.borderColor = ReadColor(path_, L"Border", next.borderColor);
    next.textColor = ReadColor(path_, L"Text", next.textColor);
    next.timestampColor = ReadColor(path_, L"Timestamp", next.timestampColor);
    next.placeholderColor = ReadColor(path_, L"Placeholder", next.placeholderColor);
    next.selectionColor = ReadColor(path_, L"Selection", next.selectionColor);

    settings_ = next;
    GetWriteTime(path_, lastWriteTime_);
    ++generation_;
    Logger::Info("Custom Chat settings loaded. generation=%llu", static_cast<unsigned long long>(generation_));
    return true;
}

bool ChatSettingsStore::Save(const ChatSettings& settings) {
    wchar_t buf[64]{};

    swprintf_s(buf, L"%.2f", settings.scale);
    WritePrivateProfileStringW(L"Layout", L"Scale", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.width);
    WritePrivateProfileStringW(L"Layout", L"Width", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.height);
    WritePrivateProfileStringW(L"Layout", L"Height", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.fontSize);
    WritePrivateProfileStringW(L"Layout", L"FontSize", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.lineHeight);
    WritePrivateProfileStringW(L"Layout", L"LineHeight", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.marginLeft);
    WritePrivateProfileStringW(L"Layout", L"MarginLeft", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.marginTop);
    WritePrivateProfileStringW(L"Layout", L"MarginTop", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.opacity);
    WritePrivateProfileStringW(L"Behavior", L"Opacity", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.idleOpacity);
    WritePrivateProfileStringW(L"Behavior", L"IdleOpacity", buf, path_.c_str());

    swprintf_s(buf, L"%d", settings.fadeDelayMs);
    WritePrivateProfileStringW(L"Behavior", L"FadeDelayMs", buf, path_.c_str());

    WritePrivateProfileStringW(L"Behavior", L"Timestamps", settings.timestamps ? L"1" : L"0", path_.c_str());
    WritePrivateProfileStringW(L"Behavior", L"SmoothScroll", settings.smoothScroll ? L"1" : L"0", path_.c_str());
    WritePrivateProfileStringW(L"Behavior", L"FadeEnabled", settings.fadeEnabled ? L"1" : L"0", path_.c_str());

    swprintf_s(buf, L"%08X", settings.backgroundColor);
    WritePrivateProfileStringW(L"Colors", L"Background", buf, path_.c_str());

    swprintf_s(buf, L"%08X", settings.textColor);
    WritePrivateProfileStringW(L"Colors", L"Text", buf, path_.c_str());

    swprintf_s(buf, L"%08X", settings.timestampColor);
    WritePrivateProfileStringW(L"Colors", L"Timestamp", buf, path_.c_str());

    settings_ = settings;
    GetWriteTime(path_, lastWriteTime_);
    ++generation_;
    return true;
}

} // namespace HUB::Chat
