#pragma once

#include <d3d9.h>
#include <windows.h>

#include <string>

namespace HUB::Chat {

struct ChatSettings {
    float scale = 1.2f;
    int width = 620;
    int height = 340;
    int fontSize = 15;
    int lineHeight = 22;
    int composerHeight = 42;
    int padding = 12;
    int marginLeft = 10;
    int marginTop = 10;
    int opacity = 255;
    int idleOpacity = 220;
    int fadeDelayMs = 10000;
    int wheelLines = 3;
    bool timestamps = true;
    bool smoothScroll = true;
    bool fadeEnabled = true;
    D3DCOLOR backgroundColor = D3DCOLOR_ARGB(208, 18, 18, 20);
    D3DCOLOR composerColor = D3DCOLOR_ARGB(238, 26, 26, 30);
    D3DCOLOR borderColor = D3DCOLOR_ARGB(255, 75, 75, 80);
    D3DCOLOR textColor = D3DCOLOR_ARGB(255, 255, 255, 255);
    D3DCOLOR timestampColor = D3DCOLOR_ARGB(255, 170, 170, 170);
    D3DCOLOR placeholderColor = D3DCOLOR_ARGB(255, 187, 187, 187);
    D3DCOLOR selectionColor = D3DCOLOR_ARGB(85, 74, 144, 226);
};

class ChatSettingsStore {
public:
    bool Initialize();
    bool ReloadIfChanged();
    bool Save(const ChatSettings& settings);
    const ChatSettings& Get() const;
    uint64_t Generation() const;

private:
    bool Load();

    std::wstring path_;
    ChatSettings settings_;
    FILETIME lastWriteTime_{};
    DWORD lastCheckTick_ = 0;
    uint64_t generation_ = 0;
};

ChatSettingsStore& GetSettingsStore();

} // namespace HUB::Chat
