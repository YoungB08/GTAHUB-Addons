#include "../include/RoleComponent.hpp"
#include <fstream>
#include <filesystem>
#include <cmath>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#endif

namespace HUBRole {

static RoleComponent* s_RoleComponentInstance = nullptr;

RoleComponent* GetRoleComponent() {
    return s_RoleComponentInstance;
}

void RoleComponent::onLoad(ICore* c) {
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
        Logger::Info("[LIFECYCLE] Event handler registered with core IPlayerPool.");
    }

    running_ = true;
    workerThread_ = std::thread(&RoleComponent::workerLoop, this);
    Logger::Info("[WORKER] Background worker thread started for Timed Roles & Rainbow Ticks.");
}

void RoleComponent::onReady() {
    Logger::Info("[LIFECYCLE] RoleComponent is ready and listening to server player events.");
    if (core_) {
        core_->printLn("[HUB-Role] RoleComponent is ready and listening to server player events.");
    }
}

void RoleComponent::free() {
    Logger::Info("[LIFECYCLE] RoleComponent component unloading...");
    running_ = false;
    if (workerThread_.joinable()) {
        workerThread_.join();
        Logger::Info("[WORKER] Worker thread joined and stopped successfully.");
    }

    if (core_) {
        core_->getPlayers().getPlayerConnectDispatcher().removeEventHandler(this);
    }
    s_RoleComponentInstance = nullptr;
    playerRoles_.clear();
    registeredResources_.clear();
    Logger::Info("[LIFECYCLE] RoleComponent fully unloaded.");
}

void RoleComponent::reset() {
    std::lock_guard<std::mutex> lock(lock_);
    Logger::Info("[SYSTEM] Resetting all stored player roles, resources, and configurations.");
    playerRoles_.clear();
    registeredResources_.clear();
    config_ = GlobalConfig{};
}

void RoleComponent::onPlayerConnect(IPlayer& player) {
    std::lock_guard<std::mutex> lock(lock_);
    int id = player.getID();
    playerRoles_[id].reset();
    Logger::Info("[PLAYER CONNECT] Player ID %d connected. Role state initialized.", id);
}

void RoleComponent::onPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason) {
    (void)reason;
    std::lock_guard<std::mutex> lock(lock_);
    int id = player.getID();
    playerRoles_.erase(id);
    Logger::Info("[PLAYER DISCONNECT] Player ID %d disconnected. Role state cleared.", id);
}

// -----------------------------------------------------------------------------
// 1. GLOBAL CONFIG & ASYNC RESOURCE
// -----------------------------------------------------------------------------

bool RoleComponent::setRoleGlobalConfig(float distance, bool enableLOS, bool autoHideInVeh, float iconWidth, float iconHeight) {
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

    if (key.empty()) {
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
        std::thread downloadThread(&RoleComponent::downloadResourceAsync, this, key, sUrl, localPath);
        downloadThread.detach();
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

    triggerOnRoleResourceLoaded(-1, key, success);
}

// -----------------------------------------------------------------------------
// 2. CORE SET ROLE NATIVES (Multi-Slot & Timed)
// -----------------------------------------------------------------------------

bool RoleComponent::setPlayerPresetRole(int toPlayer, int targetPlayer, uint8_t presetRole, int slotID, int durationSeconds) {
    if (slotID < 0 || slotID >= MAX_ROLE_SLOTS) {
        Logger::Warn("[SET ROLE] Invalid slotID %d for targetPlayer %d.", slotID, targetPlayer);
        return false;
    }

    std::lock_guard<std::mutex> lock(lock_);
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

    broadcastRoleUpdate(toPlayer, targetPlayer, state);
    return true;
}

bool RoleComponent::setPlayerCustomRole(int toPlayer, int targetPlayer, std::string_view tagText, uint32_t color, uint32_t bgColor, bool stroke, int slotID, int durationSeconds) {
    if (slotID < 0 || slotID >= MAX_ROLE_SLOTS) return false;

    std::lock_guard<std::mutex> lock(lock_);
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

    broadcastRoleUpdate(toPlayer, targetPlayer, state);
    return true;
}

bool RoleComponent::setPlayerImageRole(int toPlayer, int targetPlayer, std::string_view resourceKey, std::string_view tagText, uint32_t color, int slotID, int durationSeconds) {
    if (slotID < 0 || slotID >= MAX_ROLE_SLOTS) return false;

    std::lock_guard<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    auto& slot = state.slots[slotID];
    slot = RoleSlotData{};
    slot.active = true;
    slot.text = std::string(tagText);
    slot.color = color;
    slot.resourceKey = std::string(resourceKey);
    slot.imagePath = "HUB-Core/icons/" + slot.resourceKey;

    if (durationSeconds > 0) {
        slot.isTimed = true;
        slot.expireTime = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    }

    Logger::Info("[SET IMAGE] targetPlayer=%d, resourceKey='%s', imagePath='%s', text='%s', slotID=%d, duration=%ds, toPlayer=%d",
                 targetPlayer, slot.resourceKey.c_str(), slot.imagePath.c_str(), slot.text.c_str(), slotID, durationSeconds, toPlayer);

    broadcastRoleUpdate(toPlayer, targetPlayer, state);
    return true;
}

// -----------------------------------------------------------------------------
// 3. RAINBOW EFFECTS
// -----------------------------------------------------------------------------

bool RoleComponent::setPlayerRainbowRole(int targetPlayer, bool toggle, int speed_ms, int slotID) {
    std::lock_guard<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];

    if (slotID == -1) {
        state.isNametagRainbow = toggle;
        state.nametagRainbowSpeedMs = (speed_ms > 50) ? speed_ms : 500;
        Logger::Info("[RAINBOW] SetPlayerRainbowRole: targetPlayer=%d, MainNametag=%d, speed=%dms",
                     targetPlayer, toggle ? 1 : 0, state.nametagRainbowSpeedMs);
    } else if (slotID >= 0 && slotID < MAX_ROLE_SLOTS) {
        auto& slot = state.slots[slotID];
        slot.isRainbow = toggle;
        slot.rainbowSpeedMs = (speed_ms > 50) ? speed_ms : 500;
        Logger::Info("[RAINBOW] SetPlayerRainbowRole: targetPlayer=%d, slotID=%d, toggle=%d, speed=%dms",
                     targetPlayer, slotID, toggle ? 1 : 0, slot.rainbowSpeedMs);
    } else {
        return false;
    }

    broadcastRoleUpdate(-1, targetPlayer, state);
    return true;
}

bool RoleComponent::isPlayerRainbowActive(int playerid, int slotID) const {
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
    std::lock_guard<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    state.nametagColor = color;
    state.hasCustomNametagColor = true;

    Logger::Info("[NAMETAG COLOR] SetPlayerNametagColor: targetPlayer=%d, color=0x%08X, toPlayer=%d",
                 targetPlayer, color, toPlayer);

    broadcastRoleUpdate(toPlayer, targetPlayer, state);
    return true;
}

bool RoleComponent::getPlayerNametagColor(int targetPlayer, uint32_t& color) const {
    std::lock_guard<std::mutex> lock(lock_);
    auto it = playerRoles_.find(targetPlayer);
    if (it != playerRoles_.end() && it->second.hasCustomNametagColor) {
        color = it->second.nametagColor;
        return true;
    }
    return false;
}

bool RoleComponent::clearPlayerRole(int toPlayer, int targetPlayer, int slotID) {
    std::lock_guard<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];

    if (slotID == -1) {
        state.reset();
        Logger::Info("[CLEAR ROLE] ClearPlayerRole: ALL slots cleared for targetPlayer=%d, toPlayer=%d", targetPlayer, toPlayer);
    } else if (slotID >= 0 && slotID < MAX_ROLE_SLOTS) {
        state.slots[slotID] = RoleSlotData{};
        Logger::Info("[CLEAR ROLE] ClearPlayerRole: Slot %d cleared for targetPlayer=%d, toPlayer=%d", slotID, targetPlayer, toPlayer);
    } else {
        return false;
    }

    broadcastRoleUpdate(toPlayer, targetPlayer, state);
    return true;
}

bool RoleComponent::setPlayerRoleVisible(int targetPlayer, bool toggle, int toPlayer) {
    std::lock_guard<std::mutex> lock(lock_);
    auto& state = playerRoles_[targetPlayer];
    state.visible = toggle;

    Logger::Info("[VISIBILITY] SetPlayerRoleVisible: targetPlayer=%d, visible=%d (Undercover/AdminDuty), toPlayer=%d",
                 targetPlayer, toggle ? 1 : 0, toPlayer);

    broadcastRoleUpdate(toPlayer, targetPlayer, state);
    return true;
}

// -----------------------------------------------------------------------------
// 5. GETTERS & CHECKERS
// -----------------------------------------------------------------------------

bool RoleComponent::hasPlayerRole(int playerid, int slotID) const {
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
    std::lock_guard<std::mutex> lock(lock_);
    auto it = playerRoles_.find(playerid);
    if (it != playerRoles_.end()) {
        return it->second.visible;
    }
    return true;
}

const PlayerRoleState* RoleComponent::getPlayerRoleState(int playerid) const {
    std::lock_guard<std::mutex> lock(lock_);
    auto it = playerRoles_.find(playerid);
    if (it != playerRoles_.end()) {
        return &it->second;
    }
    return nullptr;
}

// -----------------------------------------------------------------------------
// PAWN CALLBACKS & WORKER LOOP
// -----------------------------------------------------------------------------

void RoleComponent::triggerOnRoleResourceLoaded(int playerid, const std::string& key, bool success) {
    (void)playerid;
    (void)key;
    (void)success;
}

void RoleComponent::triggerOnPlayerRoleExpired(int playerid, int slotID) {
    (void)playerid;
    (void)slotID;
}

uint32_t RoleComponent::HSVtoARGB(float h, float s, float v, uint8_t alpha) {
    float c = v * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r = 0, g = 0, b = 0;

    if (h >= 0 && h < 60)       { r = c; g = x; b = 0; }
    else if (h >= 60 && h < 120){ r = x; g = c; b = 0; }
    else if (h >= 120 && h < 180){ r = 0; g = c; b = x; }
    else if (h >= 180 && h < 240){ r = 0; g = x; b = c; }
    else if (h >= 240 && h < 300){ r = x; g = 0; b = c; }
    else                        { r = c; g = 0; b = x; }

    uint8_t R = static_cast<uint8_t>((r + m) * 255.0f);
    uint8_t G = static_cast<uint8_t>((g + m) * 255.0f);
    uint8_t B = static_cast<uint8_t>((b + m) * 255.0f);

    return (static_cast<uint32_t>(alpha) << 24) | (static_cast<uint32_t>(R) << 16) | (static_cast<uint32_t>(G) << 8) | static_cast<uint32_t>(B);
}

void RoleComponent::workerLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(lock_);
        for (auto& [pid, state] : playerRoles_) {
            for (int s = 0; s < MAX_ROLE_SLOTS; ++s) {
                auto& slot = state.slots[s];
                if (slot.active && slot.isTimed && now >= slot.expireTime) {
                    Logger::Info("[TIMED ROLE EXPIRED] Player ID %d, Slot %d role '%s' expired. Clearing slot.", pid, s, slot.text.c_str());
                    slot = RoleSlotData{};
                    triggerOnPlayerRoleExpired(pid, s);
                    broadcastRoleUpdate(-1, pid, state);
                }
            }

            if (state.isNametagRainbow) {
                state.nametagRainbowHue = (state.nametagRainbowHue + 5) % 360;
                state.nametagColor = HSVtoARGB(static_cast<float>(state.nametagRainbowHue), 1.0f, 1.0f);
            }

            for (int s = 0; s < MAX_ROLE_SLOTS; ++s) {
                auto& slot = state.slots[s];
                if (slot.active && slot.isRainbow) {
                    slot.currentHue = (slot.currentHue + 5) % 360;
                    slot.color = HSVtoARGB(static_cast<float>(slot.currentHue), 1.0f, 1.0f);
                }
            }
        }
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
            uint8_t textLen = static_cast<uint8_t>(slot.text.length());
            buf.push_back(textLen);
            buf.insert(buf.end(), slot.text.begin(), slot.text.end());

            uint32_t color = slot.color;
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&color), reinterpret_cast<const uint8_t*>(&color) + sizeof(color));

            uint32_t bgColor = slot.bgColor;
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&bgColor), reinterpret_cast<const uint8_t*>(&bgColor) + sizeof(bgColor));

            buf.push_back(slot.stroke ? 1 : 0);

            uint8_t imgLen = static_cast<uint8_t>(slot.imagePath.length());
            buf.push_back(imgLen);
            buf.insert(buf.end(), slot.imagePath.begin(), slot.imagePath.end());
        }

        Span<uint8_t> packetSpan(buf.data(), buf.size());

        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0);
        }
    }

    // 2. Pack Packet 226 (Nametag Custom Color)
    if (state.hasCustomNametagColor) {
        std::vector<uint8_t> buf;
        buf.push_back(226); // kPktNametagColor
        uint16_t targetId = static_cast<uint16_t>(targetPlayer);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
        uint32_t color = state.nametagColor;
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&color), reinterpret_cast<const uint8_t*>(&color) + sizeof(color));

        Span<uint8_t> packetSpan(buf.data(), buf.size());
        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0);
        }
    }

    // 3. Pack Packet 227 (Visibility / Undercover mode)
    {
        std::vector<uint8_t> buf;
        buf.push_back(227); // kPktSetVisibility
        uint16_t targetId = static_cast<uint16_t>(targetPlayer);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
        buf.push_back(state.visible ? 1 : 0);

        Span<uint8_t> packetSpan(buf.data(), buf.size());
        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0);
        }
    }

    // 4. Pack Packet 225 (Rainbow Effect per slot & nametag)
    if (state.isNametagRainbow) {
        std::vector<uint8_t> buf;
        buf.push_back(225); // kPktSetRainbow
        uint16_t targetId = static_cast<uint16_t>(targetPlayer);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
        int8_t slotID = -1;
        buf.push_back(static_cast<uint8_t>(slotID));
        buf.push_back(1); // toggle
        uint16_t speed = static_cast<uint16_t>(state.nametagRainbowSpeedMs);
        buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&speed), reinterpret_cast<const uint8_t*>(&speed) + sizeof(speed));

        Span<uint8_t> packetSpan(buf.data(), buf.size());
        if (toPlayer == -1) {
            for (IPlayer* p : core_->getPlayers().entries()) {
                if (p) p->sendPacket(packetSpan, 0);
            }
        } else {
            IPlayer* p = core_->getPlayers().get(toPlayer);
            if (p) p->sendPacket(packetSpan, 0);
        }
    }

    for (int8_t s = 0; s < MAX_ROLE_SLOTS; ++s) {
        if (state.slots[s].isRainbow) {
            std::vector<uint8_t> buf;
            buf.push_back(225); // kPktSetRainbow
            uint16_t targetId = static_cast<uint16_t>(targetPlayer);
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&targetId), reinterpret_cast<const uint8_t*>(&targetId) + sizeof(targetId));
            buf.push_back(static_cast<uint8_t>(s));
            buf.push_back(1); // toggle
            uint16_t speed = static_cast<uint16_t>(state.slots[s].rainbowSpeedMs);
            buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&speed), reinterpret_cast<const uint8_t*>(&speed) + sizeof(speed));

            Span<uint8_t> packetSpan(buf.data(), buf.size());
            if (toPlayer == -1) {
                for (IPlayer* p : core_->getPlayers().entries()) {
                    if (p) p->sendPacket(packetSpan, 0);
                }
            } else {
                IPlayer* p = core_->getPlayers().get(toPlayer);
                if (p) p->sendPacket(packetSpan, 0);
            }
        }
    }
}

} // namespace HUBRole

COMPONENT_ENTRY_POINT(HUBRole::RoleComponent);
