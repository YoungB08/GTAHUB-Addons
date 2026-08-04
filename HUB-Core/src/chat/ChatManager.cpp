#include "pch.h"
#include "ChatManager.h"

#include "Logger.h"
#include "PlayerData.h"

#include <sampapi/0.3.DL-1/CNetGame.h>
#include <sampapi/0.3.DL-1/CPlayerPool.h>

#include <algorithm>
#include <cstring>
#include <limits>

using namespace sampapi::v03dl;

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

int FindPlayerIdFromChat(const std::wstring& prefix, const std::wstring& text) {
    CNetGame* netGame = GetRefNetGame();
    CPlayerPool* pool = netGame ? netGame->GetPlayerPool() : nullptr;
    if (!pool) return -1;

    std::wstring target = prefix;
    if (target.empty() && !text.empty()) {
        const size_t colonPos = text.find(L':');
        if (colonPos != std::wstring::npos && colonPos < 32) {
            target = text.substr(0, colonPos);
        }
    }

    if (target.empty()) return -1;
    while (!target.empty() && (target.back() == L':' || target.back() == L' ')) {
        target.pop_back();
    }
    if (target.empty()) return -1;

    const char* localName = pool->GetLocalPlayerName();
    if (localName) {
        const std::wstring localWide = MultiByteToWide(localName, CP_UTF8, 0);
        if (!localWide.empty() && _wcsicmp(localWide.c_str(), target.c_str()) == 0) {
            return pool->m_nLocalPlayerId;
        }
    }

    for (int id = 0; id < kMaxPlayers; ++id) {
        if (!pool->IsConnected(id)) continue;
        const char* name = pool->GetName(id);
        if (name) {
            const std::wstring nameWide = MultiByteToWide(name, CP_UTF8, 0);
            if (!nameWide.empty() && _wcsicmp(nameWide.c_str(), target.c_str()) == 0) {
                return id;
            }
        }
    }
    return -1;
}

std::wstring NormalizeWide(const wchar_t* text, int length, NORM_FORM form) {
    if (!text || length <= 0) return {};

    const int normalizedLength = NormalizeString(form, text, length, nullptr, 0);
    if (normalizedLength <= 0) return {};

    std::wstring result(static_cast<size_t>(normalizedLength), L'\0');
    const int written = NormalizeString(form, text, length, result.data(), normalizedLength);
    if (written <= 0) return {};
    result.resize(static_cast<size_t>(written));
    return result;
}

std::wstring NormalizeWide(const std::wstring& text, NORM_FORM form) {
    if (text.empty()) return {};
    return NormalizeWide(text.data(), static_cast<int>(text.size()), form);
}

std::string HexBytes(const char* text) {
    if (!text) return {};
    constexpr char digits[] = "0123456789ABCDEF";
    const size_t length = strlen(text);
    std::string result;
    result.reserve(length == 0 ? 0 : length * 3 - 1);
    for (size_t position = 0; position < length; ++position) {
        if (position != 0) result.push_back('-');
        const unsigned char value = static_cast<unsigned char>(text[position]);
        result.push_back(digits[value >> 4]);
        result.push_back(digits[value & 0x0F]);
    }
    return result;
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

    message.playerId = FindPlayerIdFromChat(message.prefix, message.text);

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

    message.playerId = FindPlayerIdFromChat(message.prefix, message.text);

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
    Logger::Input("Player sent chat input (%u bytes): %s",
        text ? static_cast<unsigned>(strlen(text)) : 0u, HexBytes(text).c_str());
}

void ChatManager::Push(ChatMessage message) {
    std::lock_guard<std::mutex> guard(mutex_);
    message.id = nextId_++;
    messages_.push_back(std::move(message));
    if (messages_.size() > kMaxMessages) {
        messages_.pop_front();
    }
    ++generation_;
}

MessageSnapshot ChatManager::Snapshot(size_t maximumCount) const {
    std::lock_guard<std::mutex> guard(mutex_);
    MessageSnapshot snapshot;
    snapshot.generation = generation_;
    if (maximumCount == 0 || messages_.empty()) return snapshot;

    const size_t count = (std::min)(maximumCount, messages_.size());
    snapshot.messages.assign(messages_.end() - static_cast<std::ptrdiff_t>(count), messages_.end());
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
    if (!text || text[0] == '\0') return {};

    std::wstring decoded = MultiByteToWide(text, CP_UTF8, MB_ERR_INVALID_CHARS);
    if (decoded.empty()) decoded = MultiByteToWide(text, 1258, 0);
    if (decoded.empty()) decoded = MultiByteToWide(text, CP_ACP, 0);
    if (decoded.empty()) return {};

    std::wstring normalized = NormalizeWide(decoded, NormalizationC);
    return normalized.empty() ? decoded : normalized;
}

std::string ChatManager::EncodeSampText(const std::wstring& text, size_t maximumBytes) {
    if (text.empty()) return {};

    std::wstring normalized = NormalizeWide(text, NormalizationD);
    const std::wstring& source = normalized.empty() ? text : normalized;

    const int required = WideCharToMultiByte(1258, 0, source.c_str(),
        static_cast<int>(source.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};

    std::string encoded(static_cast<size_t>(required), '\0');
    if (WideCharToMultiByte(1258, 0, source.c_str(), static_cast<int>(source.size()),
        encoded.data(), required, nullptr, nullptr) == 0) {
        return {};
    }

    if (encoded.size() > maximumBytes) {
        encoded.resize(maximumBytes);
    }
    return encoded;
}

std::string ChatManager::EncodeUtf8(const std::wstring& text, size_t maximumBytes) {
    if (text.empty()) return {};

    std::wstring normalized = NormalizeWide(text, NormalizationC);
    const std::wstring& source = normalized.empty() ? text : normalized;

    const int required = WideCharToMultiByte(CP_UTF8, 0, source.c_str(),
        static_cast<int>(source.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) return {};

    std::string encoded(static_cast<size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, source.c_str(), static_cast<int>(source.size()),
        encoded.data(), required, nullptr, nullptr) == 0) {
        return {};
    }

    if (encoded.size() > maximumBytes) {
        size_t fit = maximumBytes;
        while (fit > 0 && (static_cast<unsigned char>(encoded[fit]) & 0xC0) == 0x80) {
            --fit;
        }
        if (fit > 0 && (static_cast<unsigned char>(encoded[fit - 1]) & 0x80) != 0) {
            --fit;
        }
        encoded.resize(fit);
    }
    return encoded;
}

} // namespace HUB::Chat
