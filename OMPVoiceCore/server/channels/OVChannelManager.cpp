#include "OVChannelManager.h"

#include "shared/OVConstants.h"

#include <algorithm>
#include <cmath>

namespace ov::server
{
void OVChannelManager::UpsertPlayer(int playerId, const Vector3& position, int vehicleId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& player = players_[playerId];
    player.playerId = playerId;
    player.position = position;
    player.vehicleId = vehicleId;
}

void OVChannelManager::RemovePlayer(int playerId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    players_.erase(playerId);
    for (auto& [_, player] : players_)
    {
        player.mutedPlayers.erase(playerId);
        if (player.phoneTarget == playerId) player.phoneTarget = -1;
    }
}

void OVChannelManager::SetEnabled(int playerId, bool enabled) { std::lock_guard<std::mutex> lock(mutex_); players_[playerId].voiceEnabled = enabled; }
bool OVChannelManager::IsEnabled(int playerId) const { std::lock_guard<std::mutex> lock(mutex_); const auto it = players_.find(playerId); return it != players_.end() && it->second.voiceEnabled; }
bool OVChannelManager::IsTalking(int playerId) const { std::lock_guard<std::mutex> lock(mutex_); const auto it = players_.find(playerId); return it != players_.end() && it->second.transmitting; }
void OVChannelManager::SetTalking(int playerId, bool talking) { std::lock_guard<std::mutex> lock(mutex_); players_[playerId].transmitting = talking; }
void OVChannelManager::SetVolume(int playerId, float volume) { std::lock_guard<std::mutex> lock(mutex_); players_[playerId].volume = std::clamp(volume, 0.0F, 2.0F); }
void OVChannelManager::SetTalkKey(int playerId, int key) { std::lock_guard<std::mutex> lock(mutex_); players_[playerId].talkKey = std::clamp(key, 1, 255); }
void OVChannelManager::SetHudVisible(int playerId, bool visible) { std::lock_guard<std::mutex> lock(mutex_); players_[playerId].hudVisible = visible; }

void OVChannelManager::SetMuted(int listenerId, int targetId, bool mute)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto& muted = players_[listenerId].mutedPlayers;
    if (mute) muted.insert(targetId); else muted.erase(targetId);
}

std::uint32_t OVChannelManager::CreateChannel(float distance, VoiceMode mode)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto id = nextChannelId_++;
    channels_[id] = VoiceChannel{id, distance < 0.0F ? GLOBAL_DISTANCE : std::clamp(distance, 1.0F, 200.0F), distance >= 0.0F, mode};
    return id;
}

bool OVChannelManager::DestroyChannel(std::uint32_t channelId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_.erase(channelId) != 0;
}

bool OVChannelManager::StartPhoneCall(int playerId, int targetId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto source = players_.find(playerId);
    auto target = players_.find(targetId);
    if (source == players_.end() || target == players_.end() || playerId == targetId || source->second.phoneTarget != -1 || target->second.phoneTarget != -1) return false;
    source->second.phoneTarget = targetId;
    target->second.phoneTarget = playerId;
    return true;
}

void OVChannelManager::EndPhoneCall(int playerId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = players_.find(playerId);
    if (it == players_.end()) return;
    const int target = it->second.phoneTarget;
    it->second.phoneTarget = -1;
    if (const auto targetIt = players_.find(target); targetIt != players_.end()) targetIt->second.phoneTarget = -1;
}

std::uint32_t OVChannelManager::CreateRadioChannel()
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto id = nextRadioId_++;
    channels_[id] = VoiceChannel{id, GLOBAL_DISTANCE, false, VoiceMode::Radio};
    return id;
}

bool OVChannelManager::JoinRadio(int playerId, std::uint32_t channelId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (channels_.find(channelId) == channels_.end() || channels_.at(channelId).mode != VoiceMode::Radio) return false;
    players_[playerId].radioChannels.insert(channelId);
    return true;
}

bool OVChannelManager::LeaveRadio(int playerId, std::uint32_t channelId)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = players_.find(playerId);
    return it != players_.end() && it->second.radioChannels.erase(channelId) != 0;
}

std::vector<VoiceRecipient> OVChannelManager::Recipients(int sourceId, std::uint32_t channelId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const auto sourceIt = players_.find(sourceId);
    if (sourceIt == players_.end() || !sourceIt->second.voiceEnabled) return {};
    const auto channelIt = channels_.find(channelId);
    const VoiceChannel channel = channelIt == channels_.end() ? VoiceChannel{channelId, DEFAULT_VOICE_DISTANCE, true, VoiceMode::Proximity} : channelIt->second;
    std::vector<VoiceRecipient> result;
    for (const auto& [id, listener] : players_)
    {
        if (id == sourceId || !listener.voiceEnabled || listener.mutedPlayers.count(sourceId) != 0) continue;
        float gain = 0.0F;
        float pan = 0.0F;
        const float distance = sourceIt->second.position.DistanceTo(listener.position);
        const bool phoneParticipant = listener.phoneTarget == sourceId || sourceIt->second.phoneTarget == id;
        const bool phoneLeak = sourceIt->second.phoneTarget != -1 && distance <= PHONE_LEAK_RADIUS;
        const bool sameVehicle = sourceIt->second.vehicleId >= 0 && sourceIt->second.vehicleId == listener.vehicleId;
        const bool sameRadio = channel.mode == VoiceMode::Radio && listener.radioChannels.count(channelId) != 0 && sourceIt->second.radioChannels.count(channelId) != 0;
        if (phoneParticipant) gain = 1.0F;
        else if (phoneLeak) gain = 0.30F * std::max(0.0F, 1.0F - distance / PHONE_LEAK_RADIUS);
        else if (channel.mode == VoiceMode::Global || sameVehicle || (channel.mode == VoiceMode::Radio && sameRadio)) gain = 1.0F;
        else if (distance <= channel.hearDistance)
        {
            gain = distance <= 1.0F ? 1.0F : std::max(0.0F, 1.0F - (distance - 1.0F) / std::max(1.0F, channel.hearDistance - 1.0F));
            if (distance > 0.01F) pan = std::clamp((listener.position.x - sourceIt->second.position.x) / channel.hearDistance, -1.0F, 1.0F);
        }
        if (gain > 0.0F) result.push_back({id, std::clamp(gain * sourceIt->second.volume, 0.0F, 2.0F), pan});
    }
    return result;
}
}
