#pragma once

#include <d3d9.h>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace HUB::Chat {

enum class MessageSource {
    SampChat,
    SampInfo,
    SampDebug,
    Network,
    Local
};

struct ChatMessage {
    uint64_t id = 0;
    SYSTEMTIME timestamp{};
    std::wstring prefix;
    std::wstring text;
    D3DCOLOR prefixColor = 0xFFFFFFFF;
    D3DCOLOR textColor = 0xFFFFFFFF;
    MessageSource source = MessageSource::Local;
    int playerId = -1;
};

struct MessageSnapshot {
    uint64_t generation = 0;
    std::vector<ChatMessage> messages;
};

class ChatManager {
public:
    static ChatManager& Get();

    void OnSampEntry(int type, const char* text, const char* prefix,
        D3DCOLOR textColor, D3DCOLOR prefixColor);
    void OnServerMessage(const char* prefix, D3DCOLOR prefixColor, const char* text);
    void OnClientMessage(D3DCOLOR color, const char* text);
    void OnPlayerSend(const char* text);

    MessageSnapshot Snapshot(size_t maximumCount) const;
    uint64_t Generation() const;
    void Clear();

    static std::wstring DecodeSampText(const char* text);
    static std::string EncodeSampText(const std::wstring& text, size_t maximumBytes = (std::numeric_limits<size_t>::max)());
    static std::string EncodeUtf8(const std::wstring& text, size_t maximumBytes);

private:
    ChatManager();

    void Push(ChatMessage message);

    mutable std::mutex mutex_;
    std::deque<ChatMessage> messages_;
    uint64_t nextId_ = 1;
    uint64_t generation_ = 0;

    static constexpr size_t kMaxMessages = 1000;
};

} // namespace HUB::Chat
