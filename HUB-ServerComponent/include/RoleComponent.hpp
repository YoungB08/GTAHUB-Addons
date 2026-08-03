#pragma once

#include "PlayerRoleData.hpp"
#include "Logger.hpp"
#include "amx/amx.h"
#include <sdk.hpp>
#include <core.hpp>
#include <player.hpp>
#include <Server/Components/Pawn/pawn.hpp>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include <tuple>

namespace HUBRole {

class IRoleComponent : public IComponent {
public:
    PROVIDE_UID(0xFC1E26AE9C883F2A);

    virtual ~IRoleComponent() = default;

    // 1. Global Config & Async Resource
    virtual bool addRoleResource(std::string_view resourceKey, std::string_view url) = 0;
    virtual bool isRoleResourceLoaded(std::string_view resourceKey) const = 0;
    virtual bool setRoleGlobalConfig(float distance, bool enableLOS, bool autoHideInVeh, float iconWidth, float iconHeight) = 0;
    virtual GlobalConfig getRoleGlobalConfig() const = 0;

    // 2. Core Set Role Natives (Multi-Slot & Timed)
    virtual bool setPlayerPresetRole(int toPlayer, int targetPlayer, uint8_t presetRole, int slotID, int durationSeconds) = 0;
    virtual bool setPlayerCustomRole(int toPlayer, int targetPlayer, std::string_view tagText, uint32_t color, uint32_t bgColor, bool stroke, int slotID, int durationSeconds) = 0;
    virtual bool setPlayerImageRole(int toPlayer, int targetPlayer, std::string_view resourceKey, std::string_view tagText, uint32_t color, int slotID, int durationSeconds) = 0;

    // 3. Rainbow Effects
    virtual bool setPlayerRainbowRole(int targetPlayer, bool toggle, int speed_ms, int slotID) = 0;
    virtual bool isPlayerRainbowActive(int playerid, int slotID) const = 0;

    // 4. Nametag Color & Utilities
    virtual bool setPlayerNametagColor(int toPlayer, int targetPlayer, uint32_t color) = 0;
    virtual bool getPlayerNametagColor(int targetPlayer, uint32_t& color) const = 0;
    virtual bool clearPlayerRole(int toPlayer, int targetPlayer, int slotID) = 0;
    virtual bool setPlayerRoleVisible(int targetPlayer, bool toggle, int toPlayer) = 0;

    // 5. Getters & Checkers
    virtual bool hasPlayerRole(int playerid, int slotID) const = 0;
    virtual bool isPlayerRoleVisible(int playerid) const = 0;
    virtual const PlayerRoleState* getPlayerRoleState(int playerid) const = 0;
};

class RoleComponent final : public IRoleComponent,
                            public PlayerConnectEventHandler,
                            public CoreEventHandler,
                            public SingleNetworkInEventHandler {
private:
    ICore* core_ = nullptr;
    IPawnComponent* pawn_ = nullptr;
    PawnEventHandler* pawnEventHandler_ = nullptr;

    mutable std::mutex lock_;
    GlobalConfig config_;
    std::unordered_map<int, PlayerRoleState> playerRoles_;
    std::unordered_map<std::string, RoleResourceData> registeredResources_;
    struct ChatChannelData { std::string name; uint32_t color = 0xFFFFFFFF; };
    std::unordered_map<uint16_t, ChatChannelData> chatChannels_;
    std::unordered_map<AMX*, IPawnScript*> pawnScripts_;

    std::chrono::steady_clock::time_point lastExpiryCheck_{};
    std::vector<std::thread> downloadThreads_;
    std::vector<std::tuple<int, std::string, bool>> pendingResourceCallbacks_;

public:
    RoleComponent() = default;
    ~RoleComponent() override = default;

    StringView componentName() const override { return "HUB-ServerRoleComponent"; }
    SemanticVersion componentVersion() const override { return SemanticVersion(1, 0, 0, 0); }
    void onLoad(ICore* c) override;
    void onInit(IComponentList* components) override;
    void onReady() override;
    void free() override;
    void reset() override;

    void onPlayerConnect(IPlayer& player) override;
    void onPlayerDisconnect(IPlayer& player, PeerDisconnectReason reason) override;
    void onTick(Microseconds elapsed, TimePoint now) override;
    void onAmxLoad(IPawnScript& script);
    void onAmxUnload(IPawnScript& script);
    bool onReceive(IPlayer& peer, NetworkBitStream& bs) override;

    // IRoleComponent implementation
    bool addRoleResource(std::string_view resourceKey, std::string_view url) override;
    bool isRoleResourceLoaded(std::string_view resourceKey) const override;
    bool setRoleGlobalConfig(float distance, bool enableLOS, bool autoHideInVeh, float iconWidth, float iconHeight) override;
    GlobalConfig getRoleGlobalConfig() const override;

    bool setPlayerPresetRole(int toPlayer, int targetPlayer, uint8_t presetRole, int slotID, int durationSeconds) override;
    bool setPlayerCustomRole(int toPlayer, int targetPlayer, std::string_view tagText, uint32_t color, uint32_t bgColor, bool stroke, int slotID, int durationSeconds) override;
    bool setPlayerImageRole(int toPlayer, int targetPlayer, std::string_view resourceKey, std::string_view tagText, uint32_t color, int slotID, int durationSeconds) override;

    bool setPlayerRainbowRole(int targetPlayer, bool toggle, int speed_ms, int slotID) override;
    bool isPlayerRainbowActive(int playerid, int slotID) const override;

    bool setPlayerNametagColor(int toPlayer, int targetPlayer, uint32_t color) override;
    bool getPlayerNametagColor(int targetPlayer, uint32_t& color) const override;
    bool clearPlayerRole(int toPlayer, int targetPlayer, int slotID) override;
    bool setPlayerRoleVisible(int targetPlayer, bool toggle, int toPlayer) override;

    bool hasPlayerRole(int playerid, int slotID) const override;
    bool isPlayerRoleVisible(int playerid) const override;
    const PlayerRoleState* getPlayerRoleState(int playerid) const override;
    IPawnScript* getPawnScript(AMX* amx) const;

    bool addChatChannel(uint16_t channelId, std::string_view name, uint32_t color);
    bool removeChatChannel(uint16_t channelId);
    bool sendChatMessage(int toPlayer, uint16_t channelId, uint32_t color, std::string_view message);
    bool setPlayerChatChannel(int playerId, uint16_t channelId);
    bool clearPlayerChatChannel(int playerId, uint16_t channelId);

    // Pawn Callbacks helper
    void triggerOnRoleResourceLoaded(int playerid, const std::string& key, bool success);
    void triggerOnPlayerRoleExpired(int playerid, int slotID);

private:
    void downloadResourceAsync(std::string key, std::string url, std::string localPath);
    void broadcastRoleUpdate(int toPlayer, int targetPlayer, const PlayerRoleState& state);
    void sendChatChannels(int toPlayer);
};

RoleComponent* GetRoleComponent();
void RegisterRoleNatives(IPawnScript& script);

} // namespace HUBRole
