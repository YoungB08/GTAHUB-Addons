#pragma once

#include <d3d9.h>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

struct ChatMessage {
    std::string prefix;
    std::string text;
    D3DCOLOR prefixColor;
    D3DCOLOR textColor;
    DWORD tick;
};

class ChatManager {
public:
    static ChatManager& Get();

    void OnServerMessage(const char* szPrefix, D3DCOLOR prefixColor, const char* szText);
    void OnClientMessage(D3DCOLOR color, const char* szText);
    void OnPlayerSend(const char* szText);

    void OpenInput();
    void CloseInput();
    bool IsInputOpen() const;

    std::vector<ChatMessage> GetMessages();
    void Clear();

private:
    ChatManager();
    ~ChatManager() = default;

    mutable std::mutex m_Mutex;
    std::deque<ChatMessage> m_Messages;
    bool m_InputOpen = false;

    static constexpr size_t kMaxMessages = 300;
};
