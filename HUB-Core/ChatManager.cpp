#include "pch.h"
#include "ChatManager.h"

#include "Logger.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace {

std::wstring MultiByteToWide(const char* text, UINT codePage, DWORD flags) {
    if (!text || text[0] == '\0') return {};

    const int length = MultiByteToWideChar(codePage, flags, text, -1, nullptr, 0);
    if (length <= 1) return {};

    std::wstring result(static_cast<size_t>(length), L'\0');
    if (MultiByteToWideChar(codePage, flags, text, -1, result.data(), length) == 0) return {};
    result.pop_back();
    return result;
}

size_t Utf16CodePointLength(const std::wstring& text, size_t position) {
    if (position >= text.size()) return 0;
    const wchar_t first = text[position];
    if (first >= 0xD800 && first <= 0xDBFF && position + 1 < text.size()) {
        const wchar_t second = text[position + 1];
        if (second >= 0xDC00 && second <= 0xDFFF) return 2;
    }
    return 1;
}

} // namespace

namespace HUB::Chat {

ChatManager& ChatManager::Get() {
    static ChatManager instance;
    return instance;
}

ChatManager::ChatManager() {
    Logger::Info("ChatManager initialized.");
}

void ChatManager::OnSampEntry(int type, const char* text, const char* prefix,
    D3DCOLOR textColor, D3DCOLOR prefixColor) {
    ChatMessage message;
    message.prefix = DecodeSampText(prefix);
    message.text = DecodeSampText(text);
    if (message.prefix.empty() && message.text.empty()) return;

    GetLocalTime(&message.timestamp);
    message.prefixColor = prefixColor;
    message.textColor = textColor;
    switch (type) {
        case 2: message.source = MessageSource::SampChat; break;
        case 8: message.source = MessageSource::SampDebug; break;
        default: message.source = MessageSource::SampInfo; break;
    }
    Push(std::move(message));
}

void ChatManager::OnServerMessage(const char* prefix, D3DCOLOR prefixColor, const char* text) {
    ChatMessage message;
    message.prefix = DecodeSampText(prefix);
    message.text = DecodeSampText(text);
    if (message.prefix.empty() && message.text.empty()) return;

    GetLocalTime(&message.timestamp);
    message.prefixColor = prefixColor;
    message.textColor = 0xFFFFFFFF;
    message.source = MessageSource::Network;
    Push(std::move(message));
}

void ChatManager::OnClientMessage(D3DCOLOR color, const char* text) {
    ChatMessage message;
    message.text = DecodeSampText(text);
    if (message.text.empty()) return;

    GetLocalTime(&message.timestamp);
    message.prefixColor = color;
    message.textColor = color;
    message.source = MessageSource::Network;
    Push(std::move(message));
}

void ChatManager::OnPlayerSend(const char* text) {
    Logger::Input("Player sent chat input (%u bytes).", text ? static_cast<unsigned>(strlen(text)) : 0u);
}

MessageSnapshot ChatManager::Snapshot(size_t maximumCount) const {
    std::lock_guard<std::mutex> guard(mutex_);
    MessageSnapshot snapshot;
    snapshot.generation = generation_;

    const size_t count = (std::min)(maximumCount, messages_.size());
    snapshot.messages.reserve(count);
    const auto first = messages_.end() - static_cast<std::ptrdiff_t>(count);
    snapshot.messages.assign(first, messages_.end());
    return snapshot;
}

uint64_t ChatManager::Generation() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return generation_;
}

void ChatManager::Clear() {
    std::lock_guard<std::mutex> guard(mutex_);
    messages_.clear();
    ++generation_;
}

std::wstring ChatManager::DecodeSampText(const char* text) {
    std::wstring result = MultiByteToWide(text, CP_UTF8, MB_ERR_INVALID_CHARS);
    if (!result.empty() || !text || text[0] == '\0') return result;
    return MultiByteToWide(text, CP_ACP, 0);
}

std::string ChatManager::EncodeUtf8(const std::wstring& text, size_t maximumBytes) {
    std::string result;
    result.reserve((std::min)(maximumBytes, text.size() * 3));

    for (size_t position = 0; position < text.size();) {
        const size_t codeUnits = Utf16CodePointLength(text, position);
        char encoded[4]{};
        const int byteCount = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            text.data() + position, static_cast<int>(codeUnits), encoded,
            static_cast<int>(sizeof(encoded)), nullptr, nullptr);
        if (byteCount <= 0) {
            position += codeUnits;
            continue;
        }
        if (result.size() + static_cast<size_t>(byteCount) > maximumBytes) break;
        result.append(encoded, static_cast<size_t>(byteCount));
        position += codeUnits;
    }
    return result;
}

void ChatManager::Push(ChatMessage message) {
    const size_t textLength = message.text.size();
    {
        std::lock_guard<std::mutex> guard(mutex_);
        message.id = nextId_++;
        messages_.push_back(std::move(message));
        if (messages_.size() > kMaxMessages) messages_.pop_front();
        ++generation_;
    }
    Logger::Chat("Captured chat message (%u UTF-16 units).", static_cast<unsigned>(textLength));
}

} // namespace HUB::Chat
