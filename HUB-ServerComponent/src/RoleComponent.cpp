#include "../include/RoleComponent.hpp"
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#endif

namespace HUBRole {

static RoleComponent* s_RoleComponentInstance = nullptr;

class RolePawnEventHandler final : public PawnEventHandler {
public:
    explicit RolePawnEventHandler(RoleComponent& component) : component_(component) {}

    void onAmxLoad(IPawnScript& script) override { component_.onAmxLoad(script); }
    void onAmxUnload(IPawnScript& script) override { component_.onAmxUnload(script); }

private:
    RoleComponent& component_;
};

static bool IsValidPlayerId(int playerId) {
    return playerId >= 0 && playerId < 1004;
}

static bool IsValidRecipientId(int playerId) {
    return playerId == -1 || IsValidPlayerId(playerId);
}

RoleComponent* GetRoleComponent() {
    return s_RoleComponentInstance;
}

void RoleComponent::onLoad(ICore* c) {
#if defined(_WIN32) || defined(_WIN64)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    core_ = c;
    s_RoleComponentInstance = this;
    Logger::Init();
    Logger::Info("=================================================================");
    Logger::Info("[LIFECYCLE] RoleComponent plugin loaded into open:mp server.");
    Logger::Info("=================================================================");
    if (core_) {
        core_->printLn("[HUB-Role] RoleComponent loaded successfully.");
    }
}

void RoleComponent::onInit(IComponentList* components) {
    if (components) {
        pawn_ = components->queryComponent<IPawnComponent>();
    }

    if (core_) {
        core_->getPlayers().getPlayerConnectDispatcher().addEventHandler(this);
        core_->getEventDispatcher().addEventHandler(this);
        core_->addPerPacketInEventHandler<221>(this);
        Logger::Info("[LIFECYCLE] Event handler registered with core IPlayerPool.");
    }

    if (pawn_) {
        pawnEventHandler_ = new RolePawnEventHandler(*this);
        pawn_->getEventDispatcher().addEventHandler(pawnEventHandler_);
    }
    lastExpiryCheck_ = std::chrono::steady_clock::now();
}

void RoleComponent::onReady() {
    Logger::Info("[LIFECYCLE] RoleComponent is ready and listening to server player events.");
    if (core_) {
        core_->printLn("[HUB-Role] RoleComponent is ready and listening to server player events.");
    }
}

void RoleComponent::free() {
    Logger::Info("[LIFECYCLE] RoleComponent component unloading...");
    for (std::thread& thread : downloadThreads_) {
        if (thread.joinable()) thread.join();
    }
    downloadThreads_.clear();

    if (core_) {
        core_->getPlayers().getPlayerConnectDispatcher().removeEventHandler(this);
        core_->getEventDispatcher().removeEventHandler(this);
        core_->removePerPacketInEventHandler<221>(this);
    }
    if (pawn_ && pawnEventHandler_) pawn_->getEventDispatcher().removeEventHandler(pawnEventHandler_);
    delete static_cast<RolePawnEventHandler*>(pawnEventHandler_);
    pawnEventHandler_ = nullptr;
    s_RoleComponentInstance = nullptr;
    playerRoles_.clear();
    registeredResources_.clear();
    Logger::Info("[LIFECYCLE] RoleComponent fully unloaded.");
    delete this;
}

void RoleComponent::reset() {
    std::lock_guard<std::mutex> lock(lock_);
    Logger::Info("[SYSTEM] Resetting all stored player roles, resources, and configurations.");
    playerRoles_.clear();
    registeredResources_.clear();
    config_ = GlobalConfig{};
}

void RoleComponent::onPlayerConnect(IPlayer& player) {
    const int id = player.getID();
    std::vector<std::pair<int, PlayerRoleState>> states;
    {
        std::lock_guard<std::mutex> lock(lock_);
        playerRoles_[id].reset();
        states.reserve(playerRoles_.size());
        for (const auto& [targetId, state] : playerRoles_) states.emplace_back(targetId, state);
    }
    for (const auto& [targetId, state] : states) broadcastRoleUpdate(id, targetId, state);
    Logger::Info("[PLAYER CONNECT] Player ID %d connected. Role state initialized.", id);
}

void RoleComponent::onPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason) {
    (void)reason;
    std::lock_guard<std::mutex> lock(lock_);
    int id = player.getID();
    playerRoles_.erase(id);
    Logger::Info("[PLAYER DISCONNECT] Player ID %d disconnected. Role state cleared.", id);
}

void RoleComponent::onAmxLoad(IPawnScript& script) {
    {
        std::lock_guard<std::mutex> lock(lock_);
        pawnScripts_[script.GetAMX()] = &script;
    }
    RegisterRoleNatives(script);
}

void RoleComponent::onAmxUnload(IPawnScript& script) {
    std::lock_guard<std::mutex> lock(lock_);
    pawnScripts_.erase(script.GetAMX());
}

bool RoleComponent::onReceive(IPlayer& peer, NetworkBitStream& bs) {
    (void)bs;
    std::vector<std::pair<int, PlayerRoleState>> states;
    {
        std::lock_guard<std::mutex> lock(lock_);
        states.reserve(playerRoles_.size());
        for (const auto& [targetId, state] : playerRoles_) states.emplace_back(targetId, state);
    }
    for (const auto& [targetId, state] : states) broadcastRoleUpdate(peer.getID(), targetId, state);
    return false;
}

// -----------------------------------------------------------------------------
// 1. GLOBAL CONFIG & ASYNC RESOURCE
// -----------------------------------------------------------------------------

bool RoleComponent::setRoleGlobalConfig(float distance, bool enableLOS, bool autoHideInVeh, float iconWidth, float iconHeight) {
    if (!std::isfinite(distance) || !std::isfinite(iconWidth) || !std::isfinite(iconHeight) ||
        distance < 1.0f || distance > 300.0f || iconWidth < 1.0f || iconWidth > 256.0f ||
        iconHeight < 1.0f || iconHeight > 256.0f) return false;
    std::lock_guard<std::mutex> lock(lock_);
    config_.drawDistance = distance;
    config_.enableLOS = enableLOS;
    config_.autoHideInVeh = autoHideInVeh;
    config_.iconWidth = iconWidth;
    config_.iconHeight = iconHeight;

    Logger::Info("[CONFIG] SetRoleGlobalConfig: Distance=%.1fm, LOS=%d, AutoHideVeh=%d, IconSize=%.1fx%.1f",
                 distance, enableLOS ? 1 : 0, autoHideInVeh ? 1 : 0, iconWidth, iconHeight);
    return true;
}

GlobalConfig RoleComponent::getRoleGlobalConfig() const {
    std::lock_guard<std::mutex> lock(lock_);
    return config_;
}

bool RoleComponent::addRoleResource(std::string_view resourceKey, std::string_view url) {
    std::lock_guard<std::mutex> lock(lock_);
    std::string key(resourceKey);
    std::string sUrl(url);

    if (key.empty() || key.size() > 128 || sUrl.size() > 2048 ||
        key.find("..") != std::string::npos || key.find('/') != std::string::npos || key.find('\\') != std::string::npos) {
        Logger::Warn("[RESOURCE] AddRoleResource failed: empty resourceKey.");
        return false;
    }

    std::string localPath = "HUB-Core/icons/" + key;

    RoleResourceData res;
    res.key = key;
    res.url = sUrl;
    res.localPath = localPath;
    res.isLoaded = std::filesystem::exists(localPath);

    registeredResources_[key] = res;

    Logger::Info("[RESOURCE] AddRoleResource registered: Key='%s', URL='%s', LocalPath='%s', AlreadyLoaded=%d",
                 key.c_str(), sUrl.c_str(), localPath.c_str(), res.isLoaded ? 1 : 0);

    if (!res.isLoaded && !sUrl.empty()) {
        downloadThreads_.emplace_back(&RoleComponent::downloadResourceAsync, this, key, sUrl, localPath);
    }

    return true;
}

bool RoleComponent::isRoleResourceLoaded(std::string_view resourceKey) const {
    std::lock_guard<std::mutex> lock(lock_);
    std::string key(resourceKey);
    auto it = registeredResources_.find(key);
    if (it != registeredResources_.end()) {
        return it->second.isLoaded || std::filesystem::exists(it->second.localPath);
    }
    return std::filesystem::exists("HUB-Core/icons/" + key);
}

void RoleComponent::downloadResourceAsync(std::string key, std::string url, std::string localPath) {
    Logger::Info("[ASYNC DOWNLOAD] Started downloading resource '%s' from URL '%s'...", key.c_str(), url.c_str());

    try {
        std::filesystem::path p(localPath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
    } catch (...) {}

    bool success = false;

#if defined(_WIN32) || defined(_WIN64)
    HRESULT hr = URLDownloadToFileA(NULL, url.c_str(), localPath.c_str(), 0, NULL);
    if (SUCCEEDED(hr)) {
        success = true;
    }
#endif

    {
        std::lock_guard<std::mutex> lock(lock_);
        auto it = registeredResources_.find(key);
        if (it != registeredResources_.end()) {
            it->second.isLoaded = success;
        }
    }

    if (success) {
        Logger::Info("[ASYNC DOWNLOAD] Successfully saved resource '%s' -> '%s'.", key.c_str(), localPath.c_str());
    } else {
        Logger::Warn("[ASYNC DOWNLOAD] Failed downloading resource '%s' from '%s'.", key.c_str(), url.c_str());
    }

    {
        std::lock_guard<std::mutex> lock(lock_);
        pendingResourceCallbacks_.emplace_back(-1, key, success);
    }
}

// -----------------------------------------------------------------------------
// 2. CORE SET ROLE NATIVES (Multi-Slot & Timed)
// -----------------------------------------------------------------------------

bool RoleComponent::setPlayerPresetRole(int toPlayer, int targetPlayer, uint8_t presetRole, int slotID, int durationSeconds) {
    if (!IsValidRecipientId(toPlayer) || !IsValidPlayerId(targetPlayer) ||
        presetRole < ROLE_ADMIN || presetRole > ROLE_DEVELOPER ||
        slotID < 0 || slotID >= MAX_ROLE_SLOTS || durationSeconds < 0) {
        Logger::Warn("[SET ROLE] Invalid slotID %d for targetPlayer %d.", slotID, targetPlayer);
        return false;
    }

    std::unique_lock<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    auto& slot = state.slots[slotID];
    slot = RoleSlotData{};
    slot.active = true;
    slot.presetType = presetRole;

    switch (presetRole) {
        case ROLE_ADMIN:
            slot.text = "ADMIN";
            slot.color = 0xFFB30000;
            slot.stroke = true;
            slot.resourceKey = "admin.png";
            slot.imagePath = "HUB-Core/icons/admin.png";
            break;
        case ROLE_VIP:
            slot.text = "VIP";
            slot.color = 0xFFCC9900;
            slot.stroke = false;
            slot.resourceKey = "vip.png";
            slot.imagePath = "HUB-Core/icons/vip.png";
            break;
        case ROLE_MODERATOR:
            slot.text = "MOD";
            slot.color = 0xFF0088FF;
            slot.stroke = true;
            slot.resourceKey = "mod.png";
            slot.imagePath = "HUB-Core/icons/mod.png";
            break;
        case ROLE_HELPER:
            slot.text = "HELPER";
            slot.color = 0xFF22AA22;
            slot.stroke = false;
            slot.resourceKey = "helper.png";
            slot.imagePath = "HUB-Core/icons/helper.png";
            break;
        case ROLE_DEVELOPER:
            slot.text = "DEV";
            slot.color = 0xFFAA00FF;
            slot.stroke = true;
            slot.resourceKey = "dev.png";
            slot.imagePath = "HUB-Core/icons/dev.png";
            break;
        default:
            slot.active = false;
            break;
    }

    if (durationSeconds > 0) {
        slot.isTimed = true;
        slot.expireTime = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    }

    Logger::Info("[SET PRESET] targetPlayer=%d, preset=%d ('%s'), slotID=%d, duration=%ds, toPlayer=%d",
                 targetPlayer, presetRole, slot.text.c_str(), slotID, durationSeconds, toPlayer);

    const PlayerRoleState snapshot = state;
    lock.unlock();
    broadcastRoleUpdate(toPlayer, targetPlayer, snapshot);
    return true;
}

bool RoleComponent::setPlayerCustomRole(int toPlayer, int targetPlayer, std::string_view tagText, uint32_t color, uint32_t bgColor, bool stroke, int slotID, int durationSeconds) {
    if (!IsValidRecipientId(toPlayer) || !IsValidPlayerId(targetPlayer) || tagText.empty() ||
        tagText.size() > 63 || slotID < 0 || slotID >= MAX_ROLE_SLOTS || durationSeconds < 0) return false;

    std::unique_lock<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    auto& slot = state.slots[slotID];
    slot = RoleSlotData{};
    slot.active = true;
    slot.text = std::string(tagText);
    slot.color = color;
    slot.bgColor = bgColor;
    slot.stroke = stroke;

    if (durationSeconds > 0) {
        slot.isTimed = true;
        slot.expireTime = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    }

    Logger::Info("[SET CUSTOM] targetPlayer=%d, text='%s', color=0x%08X, bgColor=0x%08X, stroke=%d, slotID=%d, duration=%ds, toPlayer=%d",
                 targetPlayer, slot.text.c_str(), color, bgColor, stroke ? 1 : 0, slotID, durationSeconds, toPlayer);

    const PlayerRoleState snapshot = state;
    lock.unlock();
    broadcastRoleUpdate(toPlayer, targetPlayer, snapshot);
    return true;
}

bool RoleComponent::setPlayerImageRole(int toPlayer, int targetPlayer, std::string_view resourceKey, std::string_view tagText, uint32_t color, int slotID, int durationSeconds) {
    if (!IsValidRecipientId(toPlayer) || !IsValidPlayerId(targetPlayer) || resourceKey.empty() ||
        resourceKey.size() > 255 || tagText.size() > 63 || slotID < 0 ||
        slotID >= MAX_ROLE_SLOTS || durationSeconds < 0) return false;

    std::unique_lock<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    auto& slot = state.slots[slotID];
    slot = RoleSlotData{};
    slot.active = true;
    slot.text = std::string(tagText);
    slot.color = color;
    slot.resourceKey = std::string(resourceKey);
    auto resource = registeredResources_.find(slot.resourceKey);
    slot.imagePath = resource != registeredResources_.end() && !resource->second.url.empty()
        ? resource->second.url
        : "HUB-Core/icons/" + slot.resourceKey;

    if (durationSeconds > 0) {
        slot.isTimed = true;
        slot.expireTime = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    }

    Logger::Info("[SET IMAGE] targetPlayer=%d, resourceKey='%s', imagePath='%s', text='%s', slotID=%d, duration=%ds, toPlayer=%d",
                 targetPlayer, slot.resourceKey.c_str(), slot.imagePath.c_str(), slot.text.c_str(), slotID, durationSeconds, toPlayer);

    const PlayerRoleState snapshot = state;
    lock.unlock();
    broadcastRoleUpdate(toPlayer, targetPlayer, snapshot);
    return true;
}

// -----------------------------------------------------------------------------
// 3. RAINBOW EFFECTS
// -----------------------------------------------------------------------------

bool RoleComponent::setPlayerRainbowRole(int targetPlayer, bool toggle, int speed_ms, int slotID) {
    if (!IsValidPlayerId(targetPlayer) || speed_ms < 0) return false;
    std::unique_lock<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];

    if (slotID == -1) {
        state.isNametagRainbow = toggle;
        state.nametagRainbowSpeedMs = (speed_ms >= 50) ? speed_ms : 500;
        Logger::Info("[RAINBOW] SetPlayerRainbowRole: targetPlayer=%d, MainNametag=%d, speed=%dms",
                     targetPlayer, toggle ? 1 : 0, state.nametagRainbowSpeedMs);
    } else if (slotID >= 0 && slotID < MAX_ROLE_SLOTS) {
        auto& slot = state.slots[slotID];
        slot.isRainbow = toggle;
        slot.rainbowSpeedMs = (speed_ms >= 50) ? speed_ms : 500;
        Logger::Info("[RAINBOW] SetPlayerRainbowRole: targetPlayer=%d, slotID=%d, toggle=%d, speed=%dms",
                     targetPlayer, slotID, toggle ? 1 : 0, slot.rainbowSpeedMs);
    } else {
        return false;
    }

    const PlayerRoleState snapshot = state;
    lock.unlock();
    broadcastRoleUpdate(-1, targetPlayer, snapshot);
    return true;
}

bool RoleComponent::isPlayerRainbowActive(int playerid, int slotID) const {
    if (!IsValidPlayerId(playerid)) return false;
    std::lock_guard<std::mutex> lock(lock_);
    auto it = playerRoles_.find(playerid);
    if (it != playerRoles_.end()) {
        if (slotID == -1) return it->second.isNametagRainbow;
        if (slotID >= 0 && slotID < MAX_ROLE_SLOTS) return it->second.slots[slotID].isRainbow;
    }
    return false;
}

// -----------------------------------------------------------------------------
// 4. NAMETAG COLOR & UTILITIES
// -----------------------------------------------------------------------------

bool RoleComponent::setPlayerNametagColor(int toPlayer, int targetPlayer, uint32_t color) {
    if (!IsValidRecipientId(toPlayer) || !IsValidPlayerId(targetPlayer)) return false;
    std::unique_lock<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    state.nametagColor = color;
    state.hasCustomNametagColor = true;

    Logger::Info("[NAMETAG COLOR] SetPlayerNametagColor: targetPlayer=%d, color=0x%08X, toPlayer=%d",
                 targetPlayer, color, toPlayer);

    const PlayerRoleState snapshot = state;
    lock.unlock();
    broadcastRoleUpdate(toPlayer, targetPlayer, snapshot);
    return true;
}

bool RoleComponent::getPlayerNametagColor(int targetPlayer, uint32_t& color) const {
    if (!IsValidPlayerId(targetPlayer)) return false;
    std::lock_guard<std::mutex> lock(lock_);
    auto it = playerRoles_.find(targetPlayer);
    if (it != playerRoles_.end() && it->second.hasCustomNametagColor) {
        color = it->second.nametagColor;
        return true;
    }
    return false;
}

bool RoleComponent::clearPlayerRole(int toPlayer, int targetPlayer, int slotID) {
    if (!IsValidRecipientId(toPlayer) || !IsValidPlayerId(targetPlayer)) return false;
    std::unique_lock<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];

    if (slotID == -1) {
        for (auto& slot : state.slots) slot = RoleSlotData{};
        Logger::Info("[CLEAR ROLE] ClearPlayerRole: ALL slots cleared for targetPlayer=%d, toPlayer=%d", targetPlayer, toPlayer);
    } else if (slotID >= 0 && slotID < MAX_ROLE_SLOTS) {
        state.slots[slotID] = RoleSlotData{};
        Logger::Info("[CLEAR ROLE] ClearPlayerRole: Slot %d cleared for targetPlayer=%d, toPlayer=%d", slotID, targetPlayer, toPlayer);
    } else {
        return false;
    }

    const PlayerRoleState snapshot = state;
    lock.unlock();
    broadcastRoleUpdate(toPlayer, targetPlayer, snapshot);
    return true;
}

bool RoleComponent::setPlayerRoleVisible(int targetPlayer, bool toggle, int toPlayer) {
    if (!IsValidRecipientId(toPlayer) || !IsValidPlayerId(targetPlayer)) return false;
    std::unique_lock<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    state.visible = toggle;

    Logger::Info("[VISIBILITY] SetPlayerRoleVisible: targetPlayer=%d, visible=%d (Undercover/AdminDuty), toPlayer=%d",
                 targetPlayer, toggle ? 1 : 0, toPlayer);

    const PlayerRoleState snapshot = state;
    lock.unlock();
    broadcastRoleUpdate(toPlayer, targetPlayer, snapshot);
    return true;
}

// -----------------------------------------------------------------------------
// 5. GETTERS & CHECKERS
// -----------------------------------------------------------------------------

bool RoleComponent::hasPlayerRole(int playerid, int slotID) const {
    if (!IsValidPlayerId(playerid)) return false;
    std::lock_guard<std::mutex> lock(lock_);
    auto it = playerRoles_.find(playerid);
    if (it != playerRoles_.end()) {
        if (slotID >= 0 && slotID < MAX_ROLE_SLOTS) {
            return it->second.slots[slotID].active;
        }
    }
    return false;
}

bool RoleComponent::isPlayerRoleVisible(int playerid) const {
    if (!IsValidPlayerId(playerid)) return false;
    std::lock_guard<std::mutex> lock(lock_);
    auto it = playerRoles_.find(playerid);
    if (it != playerRoles_.end()) {
        return it->second.visible;
    }
    return true;
}

const PlayerRoleState* RoleComponent::getPlayerRoleState(int playerid) const {
    if (!IsValidPlayerId(playerid)) return nullptr;
    thread_local PlayerRoleState snapshot;
    std::unique_lock<std::mutex> lock(lock_);
    auto it = playerRoles_.find(playerid);
    if (it != playerRoles_.end()) {
        snapshot = it->second;
        return &snapshot;
    }
    return nullptr;
}

IPawnScript* RoleComponent::getPawnScript(AMX* amx) const {
    if (!amx) return nullptr;
    std::unique_lock<std::mutex> lock(lock_);
    const auto script = pawnScripts_.find(amx);
    return script != pawnScripts_.end() ? script->second : nullptr;
}

// -----------------------------------------------------------------------------
// PAWN CALLBACKS & WORKER LOOP
// -----------------------------------------------------------------------------

void RoleComponent::triggerOnRoleResourceLoaded(int playerid, const std::string& key, bool success) {
    if (!pawn_) return;

    auto invoke = [&](IPawnScript* script) {
        if (!script) return;
        int publicIndex = 0;
        if (script->FindPublic("OnRoleResourceLoaded", &publicIndex) != AMX_ERR_NONE) return;

        cell stringAddress = 0;
        cell* physicalAddress = nullptr;
        script->Push(success ? 1 : 0);
        script->PushString(&stringAddress, &physicalAddress, StringView(key), false, false);
        script->Push(playerid);
        cell result = 0;
        script->Exec(&result, publicIndex);
        if (stringAddress != 0) script->Release(stringAddress);
    };

    invoke(pawn_->mainScript());
    for (IPawnScript* script : pawn_->sideScripts()) invoke(script);
}

void RoleComponent::triggerOnPlayerRoleExpired(int playerid, int slotID) {
    if (!pawn_) return;

    auto invoke = [&](IPawnScript* script) {
        if (!script) return;
        int publicIndex = 0;
        if (script->FindPublic("OnPlayerRoleExpired", &publicIndex) != AMX_ERR_NONE) return;
        script->Push(slotID);
        script->Push(playerid);
        cell result = 0;
        script->Exec(&result, publicIndex);
    };

    invoke(pawn_->mainScript());
    for (IPawnScript* script : pawn_->sideScripts()) invoke(script);
}

void RoleComponent::onTick(Microseconds elapsed, TimePoint now) {
    (void)elapsed;
    (void)now;

    const auto steadyNow = std::chrono::steady_clock::now();
    if (steadyNow - lastExpiryCheck_ < std::chrono::milliseconds(100)) return;
    lastExpiryCheck_ = steadyNow;

    std::vector<std::tuple<int, int, PlayerRoleState>> expiredRoles;
    std::vector<std::tuple<int, std::string, bool>> resourceCallbacks;
    {
        std::lock_guard<std::mutex> lock(lock_);
        resourceCallbacks.swap(pendingResourceCallbacks_);
        for (auto& [playerId, state] : playerRoles_) {
            for (int slotId = 0; slotId < MAX_ROLE_SLOTS; ++slotId) {
                RoleSlotData& slot = state.slots[slotId];
                if (!slot.active || !slot.isTimed || steadyNow < slot.expireTime) continue;
                Logger::Info("[TIMED ROLE EXPIRED] Player ID %d, Slot %d role '%s' expired.",
                    playerId, slotId, slot.text.c_str());
                slot = RoleSlotData{};
                expiredRoles.emplace_back(playerId, slotId, state);
            }
        }
    }

    for (const auto& [playerId, key, success] : resourceCallbacks) {
        triggerOnRoleResourceLoaded(playerId, key, success);
    }
    for (const auto& [playerId, slotId, state] : expiredRoles) {
        triggerOnPlayerRoleExpired(playerId, slotId);
        broadcastRoleUpdate(-1, playerId, state);
    }
}

void RoleComponent::broadcastRoleUpdate(int toPlayer, int targetPlayer, const PlayerRoleState& state) {
    if (!core_) return;

    // 1. Pack Packet 220 (Nametag Slot Data)
    for (uint8_t s = 0; s < MAX_ROLE_SLOTS; ++s) {
        const auto& slot = state.slots[s];
        std::vector<uint8_t> buf;
        buf.push_back(220); // kPktNametagData
        uint16_t targetId = static_cast<uint16_t>(targetPlayer);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
        buf.push_back(s); // slotID
        buf.push_back(slot.active ? 1 : 0);

        if (slot.active) {
            uint32_t color = slot.color;
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&color), reinterpret_cast<const uint8_t*>(&color) + sizeof(color));

            uint32_t bgColor = slot.bgColor;
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&bgColor), reinterpret_cast<const uint8_t*>(&bgColor) + sizeof(bgColor));

            buf.push_back(slot.stroke ? 1 : 0);

            const size_t safeTextLength = (std::min)(slot.text.size(), size_t{63});
            buf.push_back(static_cast<uint8_t>(safeTextLength));
            buf.insert(buf.end(), slot.text.begin(), slot.text.begin() + safeTextLength);

            const size_t safeImageLength = (std::min)(slot.imagePath.size(), size_t{255});
            buf.push_back(static_cast<uint8_t>(safeImageLength));
            buf.insert(buf.end(), slot.imagePath.begin(), slot.imagePath.begin() + safeImageLength);
        }

        Span<uint8_t> packetSpan(buf.data(), buf.size() * 8);

        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0, false);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0, false);
        }
    }

    // 2. Pack Packet 226 (Nametag Custom Color)
    {
        std::vector<uint8_t> buf;
        buf.push_back(226); // kPktNametagColor
        uint16_t targetId = static_cast<uint16_t>(targetPlayer);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
        uint32_t color = state.hasCustomNametagColor ? state.nametagColor : 0xFFFFFFFF;
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&color), reinterpret_cast<const uint8_t*>(&color) + sizeof(color));

        Span<uint8_t> packetSpan(buf.data(), buf.size() * 8);
        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0, false);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0, false);
        }
    }

    // 3. Pack Packet 227 (Visibility / Undercover mode)
    {
        std::vector<uint8_t> buf;
        buf.push_back(227); // kPktSetVisibility
        uint16_t targetId = static_cast<uint16_t>(targetPlayer);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
        buf.push_back(state.visible ? 1 : 0);

        Span<uint8_t> packetSpan(buf.data(), buf.size() * 8);
        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0, false);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0, false);
        }
    }

    // 4. Pack Packet 225 (Rainbow Effect per slot & nametag)
    {
        std::vector<uint8_t> buf;
        buf.push_back(225); // kPktSetRainbow
        uint16_t targetId = static_cast<uint16_t>(targetPlayer);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
        int8_t slotID = -1;
        buf.push_back(static_cast<uint8_t>(slotID));
        buf.push_back(state.isNametagRainbow ? 1 : 0);
        uint32_t speed = state.nametagRainbowSpeedMs;
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&speed), reinterpret_cast<const uint8_t*>(&speed) + sizeof(speed));

        Span<uint8_t> packetSpan(buf.data(), buf.size() * 8);
        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0, false);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0, false);
        }
    }

    for (int8_t s = 0; s < MAX_ROLE_SLOTS; ++s) {
        {
            std::vector<uint8_t> buf;
            buf.push_back(225); // kPktSetRainbow
            uint16_t targetId = static_cast<uint16_t>(targetPlayer);
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
            buf.push_back(static_cast<uint8_t>(s));
            buf.push_back(state.slots[s].isRainbow ? 1 : 0);
            uint32_t speed = state.slots[s].rainbowSpeedMs;
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&speed), reinterpret_cast<const uint8_t*>(&speed) + sizeof(speed));

            Span<uint8_t> packetSpan(buf.data(), buf.size() * 8);
            if (toPlayer == -1) {
                for (IPlayer* p : core_->getPlayers().entries()) {
                    if (p) p->sendPacket(packetSpan, 0, false);
                }
            } else {
                IPlayer* p = core_->getPlayers().get(toPlayer);
                if (p) p->sendPacket(packetSpan, 0, false);
            }
        }
    }
}

} // namespace HUBRole

COMPONENT_ENTRY_POINT() {
    return new HUBRole::RoleComponent();
}
