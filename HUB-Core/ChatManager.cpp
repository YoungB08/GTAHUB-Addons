#include "pch.h"
#include "ChatManager.h"
#include "Logger.h"

ChatManager& ChatManager::Get() {
    static ChatManager instance;
    return instance;
}

ChatManager::ChatManager() {
    Logger::Info("ChatManager initialized.");
}

void ChatManager::OnServerMessage(const char* szPrefix, D3DCOLOR prefixColor, const char* szText) {
    const std::string prefixStr = szPrefix ? szPrefix : "";
    const std::string textStr = szText ? szText : "";
    if (textStr.empty() && prefixStr.empty()) return;

    Logger::Chat("OnServerMessage: [%s] (0x%08X) %s", prefixStr.c_str(), prefixColor, textStr.c_str());

    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        m_Messages.push_back({prefixStr, textStr, prefixColor, 0xFFFFFFFF, GetTickCount()});
        if (m_Messages.size() > kMaxMessages) {
            m_Messages.pop_front();
        }
    }
}

void ChatManager::OnClientMessage(D3DCOLOR color, const char* szText) {
    const std::string textStr = szText ? szText : "";
    if (textStr.empty()) return;

    Logger::Chat("OnClientMessage: (0x%08X) %s", color, textStr.c_str());

    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        m_Messages.push_back({"", textStr, color, color, GetTickCount()});
        if (m_Messages.size() > kMaxMessages) {
            m_Messages.pop_front();
        }
    }
}

void ChatManager::OnPlayerSend(const char* szText) {
    const std::string textStr = szText ? szText : "";
    if (textStr.empty()) return;
    Logger::Input("OnPlayerSend: %s", textStr.c_str());
}

void ChatManager::OpenInput() {
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        m_InputOpen = true;
    }
    Logger::Input("Chat input opened.");
}

void ChatManager::CloseInput() {
    {
        std::lock_guard<std::mutex> guard(m_Mutex);
        m_InputOpen = false;
    }
    Logger::Input("Chat input closed.");
}

bool ChatManager::IsInputOpen() const {
    std::lock_guard<std::mutex> guard(m_Mutex);
    return m_InputOpen;
}

std::vector<ChatMessage> ChatManager::GetMessages() {
    std::lock_guard<std::mutex> guard(m_Mutex);
    return std::vector<ChatMessage>(m_Messages.begin(), m_Messages.end());
}

void ChatManager::Clear() {
    std::lock_guard<std::mutex> guard(m_Mutex);
    m_Messages.clear();
}
