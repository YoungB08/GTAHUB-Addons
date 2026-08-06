#pragma once

#include "shared/OVConstants.h"
#include "shared/OVTypes.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ov::server
{
struct PlayerVoiceState
{
    int playerId{};
    Vector3 position{};
    int vehicleId{-1};
    bool voiceEnabled{true};
    bool transmitting{};
    bool hudVisible{true};
    int talkKey{0x5A};
    float volume{1.0F};
    int phoneTarget{-1};
    std::unordered_set<std::uint32_t> radioChannels;
    std::unordered_set<int> mutedPlayers;
};

struct VoiceRecipient
{
    int playerId{};
    float gain{1.0F};
    float pan{};
    VoiceMode mode{VoiceMode::Proximity};
};

class OVChannelManager final
{
public:
    void UpsertPlayer(int playerId, const Vector3& position, int vehicleId);
    void RemovePlayer(int playerId);
    void SetEnabled(int playerId, bool enabled);
    [[nodiscard]] bool IsEnabled(int playerId) const;
    [[nodiscard]] bool IsTalking(int playerId) const;
    void SetTalking(int playerId, bool talking);
    void SetVolume(int playerId, float volume);
    void SetMuted(int listenerId, int targetId, bool mute);
    void SetTalkKey(int playerId, int key);
    void SetHudVisible(int playerId, bool visible);
    std::uint32_t CreateChannel(float distance, VoiceMode mode = VoiceMode::Proximity);
    bool DestroyChannel(std::uint32_t channelId);
    bool StartPhoneCall(int playerId, int targetId);
    void EndPhoneCall(int playerId);
    std::uint32_t CreateRadioChannel();
    bool JoinRadio(int playerId, std::uint32_t channelId);
    bool LeaveRadio(int playerId, std::uint32_t channelId);
    void SetPhoneLeakRadius(float radius);
    [[nodiscard]] float PhoneLeakRadius() const;
    [[nodiscard]] std::vector<VoiceRecipient> Recipients(int sourceId, std::uint32_t channelId) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, PlayerVoiceState> players_;
    std::unordered_map<std::uint32_t, VoiceChannel> channels_;
    std::uint32_t nextChannelId_{1};
    std::uint32_t nextRadioId_{0x10000};
    float phoneLeakRadius_{PHONE_LEAK_RADIUS};
};
}
