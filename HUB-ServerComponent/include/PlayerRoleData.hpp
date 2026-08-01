#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <chrono>

namespace HUBRole {

constexpr int MAX_ROLE_SLOTS = 10; // Tối đa 10 slots role cho mỗi người chơi

enum PresetRole : uint8_t {
    ROLE_NONE      = 0,
    ROLE_ADMIN     = 1,
    ROLE_VIP       = 2,
    ROLE_MODERATOR = 3,
    ROLE_HELPER    = 4,
    ROLE_DEVELOPER = 5
};

struct GlobalConfig {
    float drawDistance = 20.0f;
    bool  enableLOS = true;
    bool  autoHideInVeh = true;
    float iconWidth = 32.0f;
    float iconHeight = 32.0f;
};

struct RoleSlotData {
    bool        active = false;
    std::string text;
    uint32_t    color = 0;
    uint32_t    bgColor = 0;
    bool        stroke = true;
    std::string imagePath;
    std::string resourceKey;
    uint8_t     presetType = ROLE_NONE;

    // Timed role
    bool isTimed = false;
    std::chrono::steady_clock::time_point expireTime;

    // Rainbow effect
    bool isRainbow = false;
    uint32_t rainbowSpeedMs = 500;
    uint32_t currentHue = 0;
};

struct PlayerRoleState {
    std::array<RoleSlotData, MAX_ROLE_SLOTS> slots;
    uint32_t nametagColor = 0xFFFFFFFF;
    bool     hasCustomNametagColor = false;
    bool     visible = true;

    // Rainbow for main nametag color (slotID = -1)
    bool     isNametagRainbow = false;
    uint32_t nametagRainbowSpeedMs = 500;
    uint32_t nametagRainbowHue = 0;

    void reset() {
        for (auto& s : slots) s = RoleSlotData{};
        nametagColor = 0xFFFFFFFF;
        hasCustomNametagColor = false;
        visible = true;
        isNametagRainbow = false;
        nametagRainbowSpeedMs = 500;
        nametagRainbowHue = 0;
    }
};

struct RoleResourceData {
    std::string key;
    std::string url;
    std::string localPath;
    bool        isLoaded = false;
};

} // namespace HUBRole
