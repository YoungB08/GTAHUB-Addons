#pragma once

#include <d3d9.h>
#include <windows.h>

#include <string>

namespace HUB::Chat {

struct ChatSettings {
    float scale = 1.0f;
    int width = 520;
    int height = 300;
    int fontSize = 13;
    int lineHeight = 19;
    int composerHeight = 38;
    int padding = 10;
    int marginLeft = 18;
    int marginTop = 18;
    int opacity = 235;
    int idleOpacity = 80;
    int fadeDelayMs = 10000;
    int wheelLines = 3;
    bool timestamps = true;
    bool smoothScroll = true;
    bool fadeEnabled = true;
    D3DCOLOR backgroundColor = D3DCOLOR_ARGB(255, 30, 30, 30);
    D3DCOLOR composerColor = D3DCOLOR_ARGB(255, 37, 37, 38);
    D3DCOLOR borderColor = D3DCOLOR_ARGB(255, 64, 64, 64);
    D3DCOLOR textColor = D3DCOLOR_ARGB(255, 255, 255, 255);
    D3DCOLOR timestampColor = D3DCOLOR_ARGB(255, 142, 142, 142);
    D3DCOLOR placeholderColor = D3DCOLOR_ARGB(255, 136, 136, 136);
    D3DCOLOR selectionColor = D3DCOLOR_ARGB(85, 74, 144, 226);
};

class ChatSettingsStore {
public:
    bool Initialize();
    bool ReloadIfChanged();
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

} // namespace HUB::Chat
